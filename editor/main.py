from __future__ import annotations
import ctypes, math, struct, sys
from pathlib import Path
from OpenGL.GL import *
from PySide6.QtCore import Qt, QProcess, QTimer
from PySide6.QtGui import QAction, QMatrix4x4, QSurfaceFormat, QVector3D
from PySide6.QtOpenGL import QOpenGLShader, QOpenGLShaderProgram
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtWidgets import (QApplication, QComboBox, QDockWidget, QFileDialog,
    QFormLayout, QLabel, QMainWindow, QMessageBox, QPlainTextEdit, QSpinBox,
    QTreeWidget, QTreeWidgetItem, QWidget)

VS = """#version 330 core
layout(location=0) in vec3 p; layout(location=1) in vec3 n;
uniform mat4 mvp; uniform mat4 model; out vec3 normal;
void main(){ gl_Position=mvp*vec4(p,1); normal=mat3(transpose(inverse(model)))*n; }"""
PS = """#version 330 core
in vec3 normal; out vec4 color;
void main(){ vec3 n=normalize(normal); vec3 l=normalize(vec3(-.45,.8,.55));
float d=max(dot(n,l),0); color=vec4(vec3(.22,.62,.88)*(.16+.84*d),1); }"""

def parse_obj(path: Path):
    pos, norms, out = [], [], []
    def idx(i, count): return i-1 if i > 0 else count+i
    def vertex(token):
        f=token.split("/")
        p=pos[idx(int(f[0]),len(pos))]
        n=norms[idx(int(f[2]),len(norms))] if len(f)>2 and f[2] else None
        return p,n
    for line in path.read_text(encoding="utf-8",errors="replace").splitlines():
        s=line.strip()
        if not s or s.startswith("#"): continue
        f=s.split()
        if f[0]=="v" and len(f)>=4: pos.append(tuple(map(float,f[1:4])))
        elif f[0]=="vn" and len(f)>=4: norms.append(tuple(map(float,f[1:4])))
        elif f[0]=="f" and len(f)>=4:
            poly=[vertex(x) for x in f[1:]]
            for j in range(1,len(poly)-1):
                tri=(poly[0],poly[j],poly[j+1]); a,b,c=(x[0] for x in tri)
                u=[b[k]-a[k] for k in range(3)]; v=[c[k]-a[k] for k in range(3)]
                normal=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                length=math.sqrt(sum(x*x for x in normal)) or 1
                normal=tuple(x/length for x in normal)
                for p,n in tri: out.extend((*p,*(n or normal)))
    if not out: raise ValueError("OBJ contains no renderable faces")
    return out,len(out)//6

class Viewport(QOpenGLWidget):
    def __init__(self,parent=None):
        super().__init__(parent); self.data=[]; self.count=0; self.vbo=0; self.program=None
        self.angle=0; self.distance=4
        self.timer=QTimer(self); self.timer.timeout.connect(self.tick); self.timer.start(16)
        self.setMinimumSize(480,320)
    def tick(self): self.angle=(self.angle+.35)%360; self.update()
    def set_mesh(self,data,count):
        self.data,self.count=data,count
        if self.isValid(): self.makeCurrent(); self.upload(); self.doneCurrent()
        self.update()
    def initializeGL(self):
        glEnable(GL_DEPTH_TEST); self.program=QOpenGLShaderProgram(self)
        self.program.addShaderFromSourceCode(QOpenGLShader.Vertex,VS)
        self.program.addShaderFromSourceCode(QOpenGLShader.Fragment,PS)
        if not self.program.link(): raise RuntimeError(self.program.log())
        if self.data: self.upload()
    def upload(self):
        if self.vbo: glDeleteBuffers(1,[self.vbo])
        self.vbo=glGenBuffers(1); glBindBuffer(GL_ARRAY_BUFFER,self.vbo)
        blob=struct.pack(str(len(self.data))+"f",*self.data)
        glBufferData(GL_ARRAY_BUFFER,len(blob),blob,GL_STATIC_DRAW); glBindBuffer(GL_ARRAY_BUFFER,0)
    def paintGL(self):
        glClearColor(.035,.045,.065,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT)
        if not self.program or not self.count: return
        projection=QMatrix4x4(); projection.perspective(55,max(self.width(),1)/max(self.height(),1),.05,5000)
        view=QMatrix4x4(); view.lookAt(QVector3D(0,0,self.distance),QVector3D(0,0,0),QVector3D(0,1,0))
        model=QMatrix4x4(); model.rotate(self.angle,0,1,0); model.rotate(-12,1,0,0)
        self.program.bind(); self.program.setUniformValue("mvp",projection*view*model); self.program.setUniformValue("model",model)
        glBindBuffer(GL_ARRAY_BUFFER,self.vbo); glEnableVertexAttribArray(0); glEnableVertexAttribArray(1)
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,None); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,24,ctypes.c_void_p(12))
        glDrawArrays(GL_TRIANGLES,0,self.count); glBindBuffer(GL_ARRAY_BUFFER,0); self.program.release()
    def wheelEvent(self,event):
        self.distance=max(1,min(30,self.distance-event.angleDelta().y()/1200)); self.update()

class Editor(QMainWindow):
    def __init__(self):
        super().__init__(); self.setWindowTitle("FX3.DX12 Editor 0.3.1"); self.resize(1440,900)
        self.exe=Path("C:/XWe0.2/bulid/main.exe"); self.proc=QProcess(self)
        self.proc.readyReadStandardOutput.connect(self.read_out); self.proc.readyReadStandardError.connect(self.read_err)
        self.viewport=Viewport(self); self.setCentralWidget(self.viewport)
        scene=QDockWidget("Scene",self); self.tree=QTreeWidget(); self.tree.setHeaderLabels(["Object"])
        root=QTreeWidgetItem(["Scene"]); root.addChildren([QTreeWidgetItem(["Camera"]),QTreeWidgetItem(["Directional Light"])])
        self.tree.addTopLevelItem(root); root.setExpanded(True); scene.setWidget(self.tree); self.addDockWidget(Qt.LeftDockWidgetArea,scene)
        dock=QDockWidget("Inspector / Renderer",self); panel=QWidget(); form=QFormLayout(panel)
        self.asset=QLabel("No asset loaded"); self.aa=QComboBox(); self.aa.addItems(["None","MSAA 2x","MSAA 4x","MSAA 8x"])
        self.upscaler=QComboBox(); self.upscaler.addItems(["Disabled","FSR 3 (not integrated yet)"])
        self.mode=QComboBox(); self.mode.addItems(["Rasterization","Path tracing (DXR backend pending)"])
        self.samples=QSpinBox(); self.samples.setRange(1,4096); self.samples.setValue(64)
        self.bounces=QSpinBox(); self.bounces.setRange(1,32); self.bounces.setValue(4)
        for label,widget in [("Asset",self.asset),("AA",self.aa),("Upscaler",self.upscaler),("Render mode",self.mode),("PT samples",self.samples),("Max bounces",self.bounces)]:
            form.addRow(label,widget)
        form.addRow(QLabel("FSR3 / DXR / PT controls are placeholders, not active rendering features."))
        dock.setWidget(panel); self.addDockWidget(Qt.RightDockWidgetArea,dock)
        output=QDockWidget("Output",self); self.log=QPlainTextEdit(); self.log.setReadOnly(True)
        output.setWidget(self.log); self.addDockWidget(Qt.BottomDockWidgetArea,output)
        menu=self.menuBar().addMenu("&File")
        op=QAction("Open OBJ...",self); op.setShortcut("Ctrl+O"); op.triggered.connect(self.open_obj); menu.addAction(op)
        choose=QAction("Set engine executable...",self); choose.triggered.connect(self.choose_exe); menu.addAction(choose)
        run=self.menuBar().addMenu("&Engine"); launch=QAction("Launch engine",self); launch.setShortcut("F5"); launch.triggered.connect(self.launch); run.addAction(launch)
        self.statusBar().showMessage("Ready")
    def open_obj(self):
        name,_=QFileDialog.getOpenFileName(self,"Open Wavefront OBJ","","OBJ files (*.obj)")
        if not name: return
        try:
            path=Path(name); data,count=parse_obj(path); self.viewport.set_mesh(data,count); self.asset.setText(path.name)
            self.tree.topLevelItem(0).addChild(QTreeWidgetItem([path.name])); self.log.appendPlainText(f"Loaded {path.name}: {count} vertices")
        except Exception as exc: QMessageBox.critical(self,"OBJ import error",str(exc))
    def choose_exe(self):
        name,_=QFileDialog.getOpenFileName(self,"Select engine executable",str(self.exe.parent),"Executable (*.exe)")
        if name: self.exe=Path(name); self.log.appendPlainText("Engine: "+str(self.exe))
    def launch(self):
        if not self.exe.is_file(): QMessageBox.warning(self,"Engine not found",f"Executable not found: {self.exe}"); return
        if self.proc.state()!=QProcess.NotRunning: return
        self.proc.setProgram(str(self.exe)); self.proc.setWorkingDirectory(str(self.exe.parent)); self.proc.start()
        self.log.appendPlainText("Launching "+str(self.exe))
    def read_out(self): self.log.appendPlainText(bytes(self.proc.readAllStandardOutput()).decode(errors="replace"))
    def read_err(self): self.log.appendPlainText(bytes(self.proc.readAllStandardError()).decode(errors="replace"))

def main():
    fmt=QSurfaceFormat(); fmt.setVersion(3,3); fmt.setProfile(QSurfaceFormat.CoreProfile); fmt.setDepthBufferSize(24)
    QSurfaceFormat.setDefaultFormat(fmt); app=QApplication(sys.argv); win=Editor(); win.show(); return app.exec()
if __name__=="__main__": raise SystemExit(main())

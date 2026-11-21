from __future__ import annotations
import ctypes, math, struct, sys
from pathlib import Path

from OpenGL.GL import *
from PySide6.QtCore import Qt, QProcess, QRegularExpression, QTimer
from PySide6.QtGui import (QAction, QColor, QFont, QMatrix4x4, QSyntaxHighlighter,
                           QTextCharFormat, QSurfaceFormat, QVector3D)
from PySide6.QtOpenGL import QOpenGLShader, QOpenGLShaderProgram
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtWidgets import (
    QApplication, QComboBox, QDockWidget, QFileDialog, QFormLayout, QDoubleSpinBox,
    QLabel, QMainWindow, QMessageBox, QPlainTextEdit, QSpinBox, QTabWidget,
    QTreeWidget, QTreeWidgetItem, QWidget, QVBoxLayout
)

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

class CppHighlighter(QSyntaxHighlighter):
    def __init__(self, document):
        super().__init__(document)
        self.rules=[]
        def rule(pattern, color, bold=False):
            fmt=QTextCharFormat(); fmt.setForeground(QColor(color))
            if bold: fmt.setFontWeight(QFont.Bold)
            self.rules.append((QRegularExpression(pattern),fmt))
        keywords=r"\\b(auto|bool|break|case|catch|char|class|const|constexpr|continue|default|delete|do|double|else|enum|explicit|extern|false|float|for|friend|if|inline|int|long|namespace|new|nullptr|override|private|protected|public|return|short|signed|sizeof|static|struct|switch|template|this|throw|true|try|typedef|typename|union|unsigned|using|virtual|void|volatile|while)\\b"
        rule(keywords,"#c792ea",True)
        rule(r"\\b(std|string|vector|array|unique_ptr|shared_ptr|uint32_t|uint64_t|HRESULT|HWND)\\b","#82aaff")
        rule(r"\\b[A-Z_][A-Z0-9_]{2,}\\b","#ffcb6b")
        rule(r"\\b[0-9]+(\\.[0-9]+)?[fFuUlL]*\\b","#f78c6c")
        rule(r'"([^"\\\\]|\\\\.)*"',"#c3e88d")
        rule(r"'([^'\\\\]|\\\\.)*'","#c3e88d")
        rule(r"//[^\\n]*","#697386")
    def highlightBlock(self,text):
        for regex,fmt in self.rules:
            it=regex.globalMatch(text)
            while it.hasNext():
                match=it.next(); self.setFormat(match.capturedStart(),match.capturedLength(),fmt)
        if text.lstrip().startswith("#"):
            fmt=QTextCharFormat(); fmt.setForeground(QColor("#89ddff"))
            self.setFormat(0,len(text),fmt)

class CodeTab(QWidget):
    def __init__(self,path: Path|None=None):
        super().__init__(); self.path=path
        self.editor=QPlainTextEdit(); self.editor.setFont(QFont("Cascadia Code",10))
        self.editor.setTabStopDistance(4*self.editor.fontMetrics().horizontalAdvance(" "))
        self.highlighter=CppHighlighter(self.editor.document())
        layout=QVBoxLayout(self); layout.setContentsMargins(0,0,0,0); layout.addWidget(self.editor)
        if path and path.is_file(): self.editor.setPlainText(path.read_text(encoding="utf-8",errors="replace"))
    @property
    def title(self): return self.path.name if self.path else "untitled.cpp"

class Viewport(QOpenGLWidget):
    def __init__(self,parent=None):
        super().__init__(parent); self.data=[]; self.count=0; self.vbo=0; self.program=None
        self.angle=0; self.distance=4.0
        self.transform=[0.0,0.0,0.0,0.0,0.0,0.0,1.0,1.0,1.0]
        self.timer=QTimer(self); self.timer.timeout.connect(self.tick); self.timer.start(16)
        self.setMinimumSize(420,280)
    def tick(self): self.angle=(self.angle+.25)%360; self.update()
    def set_mesh(self,data,count):
        self.data,self.count=data,count
        if self.isValid(): self.makeCurrent(); self.upload(); self.doneCurrent()
        self.update()
    def set_transform(self,values): self.transform=list(values); self.update()
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
        p=self.transform; model=QMatrix4x4()
        model.translate(p[0],p[1],p[2]); model.rotate(p[3],1,0,0); model.rotate(p[4],0,1,0); model.rotate(p[5],0,0,1)
        model.scale(p[6],p[7],p[8]); model.rotate(self.angle,0,1,0)
        self.program.bind(); self.program.setUniformValue("mvp",projection*view*model); self.program.setUniformValue("model",model)
        glBindBuffer(GL_ARRAY_BUFFER,self.vbo); glEnableVertexAttribArray(0); glEnableVertexAttribArray(1)
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,None)
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,24,ctypes.c_void_p(12))
        glDrawArrays(GL_TRIANGLES,0,self.count); glBindBuffer(GL_ARRAY_BUFFER,0); self.program.release()
    def wheelEvent(self,event):
        self.distance=max(1,min(30,self.distance-event.angleDelta().y()/1200)); self.update()

class Editor(QMainWindow):
    def __init__(self):
        super().__init__(); self.setWindowTitle("FX3.DX12 Editor 0.3.2")
        self.resize(1600,950); self.exe=Path("C:/XWe0.2/bulid/main.exe")
        self.proc=QProcess(self); self.proc.readyReadStandardOutput.connect(self.read_out); self.proc.readyReadStandardError.connect(self.read_err)
        self.viewport=Viewport(self); self.setCentralWidget(self.viewport)
        self._build_scene(); self._build_inspector(); self._build_code(); self._build_output(); self._build_menus()
        self.statusBar().showMessage("FX3 Editor 0.3.2 | F11: toggle fullscreen | F5: launch engine")
        self.showFullScreen()
    def _build_scene(self):
        dock=QDockWidget("Scene Hierarchy",self); self.tree=QTreeWidget(); self.tree.setHeaderLabels(["Object"])
        root=QTreeWidgetItem(["Scene"]); root.addChildren([QTreeWidgetItem(["Camera"]),QTreeWidgetItem(["Directional Light"])])
        self.tree.addTopLevelItem(root); root.setExpanded(True); self.tree.itemSelectionChanged.connect(self._selection_changed)
        dock.setWidget(self.tree); self.addDockWidget(Qt.LeftDockWidgetArea,dock)
    def _build_inspector(self):
        dock=QDockWidget("Object Inspector",self); panel=QWidget(); form=QFormLayout(panel)
        self.object_name=QLabel("No object selected"); form.addRow("Selected",self.object_name)
        self.fields=[]
        for group,labels in [("Position",("X","Y","Z")),("Rotation",("X","Y","Z")),("Scale",("X","Y","Z"))]:
            form.addRow(QLabel(group))
            for axis in labels:
                spin=QDoubleSpinBox(); spin.setRange(-100000,100000); spin.setDecimals(3); spin.setSingleStep(.1)
                if group=="Scale": spin.setValue(1.0)
                spin.valueChanged.connect(self._transform_changed); self.fields.append(spin); form.addRow(axis,spin)
        self.aa=QComboBox(); self.aa.addItems(["TAA (planned native pass)","MSAA 2x","MSAA 4x","MSAA 8x","FSR 3.1 AA (SDK integration pending)"])
        self.fsr=QComboBox(); self.fsr.addItems(["Native resolution","FSR 3.1 Quality","FSR 3.1 Balanced","FSR 3.1 Performance","FSR 3.1 Ultra Performance"])
        self.fg=QComboBox(); self.fg.addItems(["Frame Generation: Off","FSR 3.1 Frame Generation (SDK integration pending)"])
        form.addRow("Anti-aliasing",self.aa); form.addRow("Upscaling",self.fsr); form.addRow("Frame generation",self.fg)
        form.addRow(QLabel("FSR options are configuration UI only until FidelityFX SDK is linked to DX12 render passes."))
        dock.setWidget(panel); self.addDockWidget(Qt.RightDockWidgetArea,dock)
    def _build_code(self):
        dock=QDockWidget("C++17 Code Editor",self); self.tabs=QTabWidget(); self.tabs.setTabsClosable(True)
        self.tabs.tabCloseRequested.connect(self._close_tab); dock.setWidget(self.tabs)
        self.addDockWidget(Qt.BottomDockWidgetArea,dock); self.new_code_tab()
    def _build_output(self):
        dock=QDockWidget("Output",self); self.log=QPlainTextEdit(); self.log.setReadOnly(True)
        dock.setWidget(self.log); self.addDockWidget(Qt.BottomDockWidgetArea,dock)
        self.tabifyDockWidget(self.findChild(QDockWidget,""),dock)
    def _build_menus(self):
        file=self.menuBar().addMenu("&File")
        self._action(file,"Open OBJ...","Ctrl+O",self.open_obj)
        self._action(file,"Open C++ source...","Ctrl+Shift+O",self.open_code)
        self._action(file,"Save code","Ctrl+S",self.save_code)
        self._action(file,"New C++ tab","Ctrl+N",self.new_code_tab)
        file.addSeparator(); self._action(file,"Set engine executable...",None,self.choose_exe)
        engine=self.menuBar().addMenu("&Engine"); self._action(engine,"Launch engine","F5",self.launch)
        view=self.menuBar().addMenu("&View"); self._action(view,"Toggle fullscreen","F11",self.toggle_fullscreen)
        self._action(view,"Exit fullscreen","Esc",self.leave_fullscreen)
    def _action(self,menu,label,shortcut,callback):
        a=QAction(label,self)
        if shortcut: a.setShortcut(shortcut)
        a.triggered.connect(callback); menu.addAction(a); return a
    def toggle_fullscreen(self):
        self.showNormal() if self.isFullScreen() else self.showFullScreen()
    def leave_fullscreen(self):
        if self.isFullScreen(): self.showNormal()
    def new_code_tab(self):
        tab=CodeTab(); self.tabs.addTab(tab,tab.title); self.tabs.setCurrentWidget(tab)
    def open_code(self):
        name,_=QFileDialog.getOpenFileName(self,"Open C++ source","","C++ files (*.cpp *.cc *.cxx *.h *.hpp);;All files (*.*)")
        if not name: return
        path=Path(name)
        for i in range(self.tabs.count()):
            tab=self.tabs.widget(i)
            if tab.path==path: self.tabs.setCurrentIndex(i); return
        tab=CodeTab(path); self.tabs.addTab(tab,tab.title); self.tabs.setCurrentWidget(tab)
    def save_code(self):
        tab=self.tabs.currentWidget()
        if not isinstance(tab,CodeTab): return
        if tab.path is None:
            name,_=QFileDialog.getSaveFileName(self,"Save C++ source","","C++ source (*.cpp *.h *.hpp)")
            if not name: return
            tab.path=Path(name)
        tab.path.write_text(tab.editor.toPlainText(),encoding="utf-8")
        self.tabs.setTabText(self.tabs.currentIndex(),tab.title); self.log.appendPlainText("Saved "+str(tab.path))
    def _close_tab(self,index):
        self.tabs.removeTab(index)
        if self.tabs.count()==0: self.new_code_tab()
    def _selection_changed(self):
        item=self.tree.currentItem()
        self.object_name.setText(item.text(0) if item else "No object selected")
    def _transform_changed(self):
        self.viewport.set_transform([f.value() for f in self.fields])
    def open_obj(self):
        name,_=QFileDialog.getOpenFileName(self,"Import Wavefront OBJ","","OBJ files (*.obj)")
        if not name: return
        try:
            path=Path(name); data,count=parse_obj(path); self.viewport.set_mesh(data,count)
            self.tree.topLevelItem(0).addChild(QTreeWidgetItem([path.name]))
            self.log.appendPlainText(f"Imported {path.name}: {count} triangle vertices")
        except Exception as exc: QMessageBox.critical(self,"OBJ import error",str(exc))
    def choose_exe(self):
        name,_=QFileDialog.getOpenFileName(self,"Select engine executable",str(self.exe.parent),"Executable (*.exe)")
        if name: self.exe=Path(name)
    def launch(self):
        if not self.exe.is_file():
            QMessageBox.warning(self,"Engine not found",f"Executable not found: {self.exe}"); return
        if self.proc.state()!=QProcess.NotRunning: return
        self.proc.setProgram(str(self.exe)); self.proc.setWorkingDirectory(str(self.exe.parent)); self.proc.start()
        self.log.appendPlainText("Launching "+str(self.exe))
    def read_out(self): self.log.appendPlainText(bytes(self.proc.readAllStandardOutput()).decode(errors="replace"))
    def read_err(self): self.log.appendPlainText(bytes(self.proc.readAllStandardError()).decode(errors="replace"))

def main():
    fmt=QSurfaceFormat(); fmt.setVersion(3,3); fmt.setProfile(QSurfaceFormat.CoreProfile); fmt.setDepthBufferSize(24)
    QSurfaceFormat.setDefaultFormat(fmt); app=QApplication(sys.argv); editor=Editor(); editor.show(); return app.exec()
if __name__=="__main__": raise SystemExit(main())

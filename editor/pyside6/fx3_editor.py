#!/usr/bin/env python3
"""FX3 Scene Editor — PySide6 front end for FX3Scene v1."""
from __future__ import annotations

import json
import math
import os
import pathlib
import sys
import uuid
from dataclasses import dataclass, asdict, field
from typing import Any

from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QColor, QCloseEvent
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QTreeWidget, QTreeWidgetItem,
    QVBoxLayout, QHBoxLayout, QFormLayout, QSplitter, QLineEdit, QComboBox,
    QDoubleSpinBox, QPushButton, QLabel, QFileDialog, QMessageBox, QPlainTextEdit,
    QGroupBox, QColorDialog, QSlider, QFrame
)

FORMAT = "FX3Scene"
VERSION = 1
KINDS = ["mesh", "empty", "camera", "directional_light", "point_light"]


@dataclass
class SceneObject:
    id: str
    name: str
    type: str = "mesh"
    position: list[float] = field(default_factory=lambda: [0.0, 0.0, 0.0])
    rotation: list[float] = field(default_factory=lambda: [0.0, 0.0, 0.0])
    scale: list[float] = field(default_factory=lambda: [1.0, 1.0, 1.0])
    mesh: str = ""
    material: dict[str, Any] = field(default_factory=lambda: {
        "base_color": "#b8c8df", "metallic": 0.0, "roughness": 0.5, "texture": ""
    })


class SceneDocument:
    def __init__(self):
        self.objects: list[SceneObject] = []
        self.path = ""
        self.dirty = False
        self.new()

    def new(self):
        self.objects = [
            SceneObject("camera-main", "Main Camera", "camera"),
            SceneObject("light-key", "Key Light", "directional_light"),
        ]
        self.path, self.dirty = "", False

    def add(self, name="Mesh", kind="mesh", mesh=""):
        obj = SceneObject(str(uuid.uuid4()), name, kind, mesh=mesh)
        self.objects.append(obj)
        self.dirty = True
        return obj

    def remove(self, object_id):
        self.objects = [o for o in self.objects if o.id != object_id]
        self.dirty = True

    @staticmethod
    def vector(value, label):
        if not isinstance(value, list) or len(value) != 3:
            raise ValueError(f"{label} must contain exactly three numbers")
        result = [float(x) for x in value]
        if not all(math.isfinite(x) for x in result):
            raise ValueError(f"{label} contains a non-finite number")
        return result

    def load(self, filename):
        data = json.loads(pathlib.Path(filename).read_text(encoding="utf-8"))
        if data.get("format") != FORMAT or data.get("version") != VERSION:
            raise ValueError("Unsupported FX3Scene format/version")
        objects, seen = [], set()
        for item in data.get("objects", []):
            oid = str(item["id"])
            if not oid or oid in seen:
                raise ValueError("Object IDs must be unique and non-empty")
            seen.add(oid)
            mat = item.get("material") or {}
            objects.append(SceneObject(
                oid, str(item["name"]), str(item["type"]),
                self.vector(item["position"], "position"),
                self.vector(item["rotation"], "rotation"),
                self.vector(item["scale"], "scale"),
                str(item.get("mesh", "")), dict(mat)
            ))
        self.objects, self.path, self.dirty = objects, str(filename), False

    def save(self, filename=None):
        target = filename or self.path
        if not target:
            raise ValueError("Choose a destination file first")
        payload = {"format": FORMAT, "version": VERSION,
                   "objects": [asdict(o) for o in self.objects]}
        path = pathlib.Path(target)
        path.parent.mkdir(parents=True, exist_ok=True)
        temp = path.with_suffix(path.suffix + ".tmp")
        temp.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        os.replace(temp, path)
        self.path, self.dirty = str(path), False


class FX3Editor(QMainWindow):
    def __init__(self):
        super().__init__()
        self.doc = SceneDocument()
        self.selected_id = None
        self._loading = False
        self.setWindowTitle("FX3 PySide6 Editor 0.4.1")
        self.resize(1440, 900)
        self._make_actions()
        self._make_ui()
        self.refresh()
        self.log("PySide6 editor ready — FX3Scene v1")

    def _make_actions(self):
        bar = self.menuBar()
        file_menu = bar.addMenu("&File")
        for label, shortcut, callback in [
            ("New Scene", "Ctrl+N", self.new_scene), ("Open…", "Ctrl+O", self.open_scene),
            ("Save", "Ctrl+S", self.save_scene), ("Save As…", "", lambda: self.save_scene(True))
        ]:
            action = QAction(label, self)
            if shortcut:
                action.setShortcut(shortcut)
            action.triggered.connect(callback)
            file_menu.addAction(action)
        file_menu.addSeparator()
        quit_action = QAction("Exit", self)
        quit_action.triggered.connect(self.close)
        file_menu.addAction(quit_action)
        scene_menu = bar.addMenu("&Scene")
        for label, callback in [
            ("Add Empty", lambda: self.add_object("Empty", "empty")),
            ("Add Mesh", lambda: self.add_object("Mesh", "mesh")),
            ("Add Camera", lambda: self.add_object("Camera", "camera")),
            ("Add Directional Light", lambda: self.add_object("Directional Light", "directional_light")),
            ("Import OBJ…", self.import_obj), ("Delete Selected", self.delete_selected)
        ]:
            action = QAction(label, self)
            action.triggered.connect(callback)
            scene_menu.addAction(action)

    def _make_ui(self):
        root = QWidget()
        outer = QVBoxLayout(root)
        toolbar = QHBoxLayout()
        for label, callback in [
            ("New", self.new_scene), ("Open", self.open_scene), ("Save", self.save_scene),
            ("+ Mesh", lambda: self.add_object("Mesh", "mesh")),
            ("+ Empty", lambda: self.add_object("Empty", "empty")),
            ("Import OBJ", self.import_obj), ("Delete", self.delete_selected)
        ]:
            b = QPushButton(label)
            b.clicked.connect(callback)
            toolbar.addWidget(b)
        toolbar.addStretch(1)
        self.path_label = QLabel("Untitled")
        toolbar.addWidget(self.path_label)
        outer.addLayout(toolbar)

        split = QSplitter(Qt.Orientation.Horizontal)
        left = QWidget()
        ll = QVBoxLayout(left)
        ll.addWidget(QLabel("SCENE OUTLINER"))
        self.tree = QTreeWidget()
        self.tree.setHeaderLabels(["Object", "Type"])
        self.tree.setAlternatingRowColors(True)
        self.tree.currentItemChanged.connect(self.on_select)
        ll.addWidget(self.tree)
        split.addWidget(left)

        center = QWidget()
        cl = QVBoxLayout(center)
        cl.addWidget(QLabel("VIEWPORT / SCENE PREVIEW"))
        self.preview = QFrame()
        self.preview.setFrameShape(QFrame.Shape.StyledPanel)
        self.preview.setStyleSheet("background:#171d27;color:#d8e2f0;")
        pv = QVBoxLayout(self.preview)
        self.preview_title = QLabel("FX3 Scene")
        self.preview_title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.preview_title.setStyleSheet("font-size:22px;font-weight:600;color:#e8eef7;")
        pv.addStretch(1)
        pv.addWidget(self.preview_title)
        self.preview_info = QLabel("Document preview — not a live DirectX 12 render target")
        self.preview_info.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.preview_info.setStyleSheet("color:#91a0b4;")
        pv.addWidget(self.preview_info)
        pv.addStretch(1)
        cl.addWidget(self.preview, 1)
        split.addWidget(center)

        right = QWidget()
        rl = QVBoxLayout(right)
        rl.addWidget(QLabel("INSPECTOR"))
        self.name_edit = QLineEdit()
        self.kind_combo = QComboBox()
        self.kind_combo.addItems(KINDS)
        self.mesh_edit = QLineEdit()
        self.mesh_edit.setPlaceholderText("Mesh asset path")
        self.mesh_browse = QPushButton("Browse mesh…")
        self.mesh_browse.clicked.connect(self.browse_mesh)
        rl.addWidget(self._group("Object", self._form(("Name", self.name_edit), ("Type", self.kind_combo),
                                                       ("Mesh", self.mesh_edit))))
        transform = QWidget()
        tf = QFormLayout(transform)
        self.transform = {}
        for group, defaults in [("Position", [0,0,0]), ("Rotation", [0,0,0]), ("Scale", [1,1,1])]:
            row = QWidget()
            row_l = QHBoxLayout(row)
            row_l.setContentsMargins(0,0,0,0)
            self.transform[group] = []
            for axis, default in zip("XYZ", defaults):
                spin = QDoubleSpinBox()
                spin.setRange(-1_000_000, 1_000_000)
                spin.setDecimals(4)
                spin.setSingleStep(0.1)
                spin.setValue(default)
                spin.setPrefix(axis + " ")
                row_l.addWidget(spin)
                self.transform[group].append(spin)
            tf.addRow(group, row)
        apply_transform = QPushButton("Apply Transform")
        apply_transform.clicked.connect(self.apply_transform)
        tf.addRow(apply_transform)
        rl.addWidget(self._group("Transform", transform))

        material = QWidget()
        mf = QFormLayout(material)
        self.color_button = QPushButton("Choose base color…")
        self.color_button.clicked.connect(self.choose_color)
        self.color_value = "#b8c8df"
        self.metal = QDoubleSpinBox()
        self.rough = QDoubleSpinBox()
        for spin in (self.metal, self.rough):
            spin.setRange(0, 1)
            spin.setSingleStep(0.05)
            spin.setDecimals(3)
        mf.addRow("Base color", self.color_button)
        mf.addRow("Metallic", self.metal)
        mf.addRow("Roughness", self.rough)
        self.texture_label = QLabel("No texture assigned")
        self.texture_label.setWordWrap(True)
        texture_row = QWidget()
        tr = QHBoxLayout(texture_row)
        tr.setContentsMargins(0,0,0,0)
        assign = QPushButton("Assign…")
        assign.clicked.connect(self.assign_texture)
        clear = QPushButton("Clear")
        clear.clicked.connect(self.clear_texture)
        tr.addWidget(assign)
        tr.addWidget(clear)
        mf.addRow("Texture", self.texture_label)
        mf.addRow(texture_row)
        apply_material = QPushButton("Apply Material")
        apply_material.clicked.connect(self.apply_material)
        mf.addRow(apply_material)
        rl.addWidget(self._group("Material", material))
        rl.addStretch(1)
        split.addWidget(right)
        split.setSizes([300, 700, 400])
        outer.addWidget(split, 1)

        self.console = QPlainTextEdit()
        self.console.setReadOnly(True)
        self.console.setMaximumBlockCount(500)
        self.console.setFixedHeight(115)
        outer.addWidget(QLabel("CONSOLE"))
        outer.addWidget(self.console)
        self.setCentralWidget(root)
        self.statusBar().showMessage("Ready")

    @staticmethod
    def _form(*pairs):
        w = QWidget()
        f = QFormLayout(w)
        for label, widget in pairs:
            f.addRow(label, widget)
        return w

    @staticmethod
    def _group(title, child):
        box = QGroupBox(title)
        layout = QVBoxLayout(box)
        layout.addWidget(child)
        return box

    def log(self, message):
        self.console.appendPlainText(message)

    def selected(self):
        return next((o for o in self.doc.objects if o.id == self.selected_id), None)

    def refresh(self, select_id=None):
        self._loading = True
        self.tree.clear()
        root = QTreeWidgetItem(["Scene", "root"])
        self.tree.addTopLevelItem(root)
        for obj in self.doc.objects:
            item = QTreeWidgetItem([obj.name, obj.type])
            item.setData(0, Qt.ItemDataRole.UserRole, obj.id)
            root.addChild(item)
            if obj.id == (select_id or self.selected_id):
                self.tree.setCurrentItem(item)
        root.setExpanded(True)
        self._loading = False
        self.update_title()
        self.update_preview()
        if self.selected():
            self.populate_inspector()
        else:
            self.set_inspector_enabled(False)

    def update_title(self):
        path = self.doc.path or "Untitled"
        self.path_label.setText(path)
        self.setWindowTitle(f"FX3 PySide6 Editor 0.4.1 — {path}{' *' if self.doc.dirty else ''}")
        self.statusBar().showMessage(f"{len(self.doc.objects)} objects | {'Modified' if self.doc.dirty else 'Saved'}")

    def update_preview(self):
        self.preview_title.setText("FX3 Scene\n" + (pathlib.Path(self.doc.path).name if self.doc.path else "Untitled"))
        self.preview_info.setText(f"{len(self.doc.objects)} scene objects  •  document preview only")

    def on_select(self, item, _previous):
        if self._loading:
            return
        self.selected_id = item.data(0, Qt.ItemDataRole.UserRole) if item else None
        if self.selected():
            self.populate_inspector()
        else:
            self.set_inspector_enabled(False)

    def set_inspector_enabled(self, enabled):
        for widget in [self.name_edit, self.kind_combo, self.mesh_edit, self.mesh_browse,
                       self.color_button, self.metal, self.rough, self.texture_label]:
            widget.setEnabled(enabled)
        for row in self.transform.values():
            for spin in row:
                spin.setEnabled(enabled)

    def populate_inspector(self):
        obj = self.selected()
        if not obj:
            return
        self.set_inspector_enabled(True)
        self._loading = True
        self.name_edit.setText(obj.name)
        self.kind_combo.setCurrentText(obj.type if obj.type in KINDS else "mesh")
        self.mesh_edit.setText(obj.mesh)
        for group, values in zip(("Position", "Rotation", "Scale"), (obj.position, obj.rotation, obj.scale)):
            for spin, value in zip(self.transform[group], values):
                spin.setValue(value)
        mat = obj.material
        self.color_value = str(mat.get("base_color", "#b8c8df"))
        self.color_button.setText(self.color_value)
        self.color_button.setStyleSheet(f"background:{self.color_value};")
        self.metal.setValue(float(mat.get("metallic", 0)))
        self.rough.setValue(float(mat.get("roughness", 0.5)))
        self.texture_label.setText(str(mat.get("texture", "")) or "No texture assigned")
        self._loading = False

    def mark_dirty(self):
        self.doc.dirty = True
        self.update_title()

    def add_object(self, name, kind):
        obj = self.doc.add(name, kind)
        self.selected_id = obj.id
        self.refresh(obj.id)
        self.log(f"Created {kind}: {name}")

    def delete_selected(self):
        obj = self.selected()
        if not obj:
            return
        self.doc.remove(obj.id)
        self.selected_id = None
        self.refresh()
        self.log(f"Deleted {obj.name}")

    def apply_transform(self):
        obj = self.selected()
        if not obj:
            return
        obj.position = [s.value() for s in self.transform["Position"]]
        obj.rotation = [s.value() for s in self.transform["Rotation"]]
        obj.scale = [s.value() for s in self.transform["Scale"]]
        self.mark_dirty()
        self.log(f"Updated transform: {obj.name}")

    def apply_material(self):
        obj = self.selected()
        if not obj:
            return
        obj.material.update({"base_color": self.color_value, "metallic": self.metal.value(),
                             "roughness": self.rough.value()})
        self.mark_dirty()
        self.log(f"Applied material to {obj.name}")

    def choose_color(self):
        color = QColorDialog.getColor(QColor(self.color_value), self, "Base color")
        if color.isValid():
            self.color_value = color.name()
            self.color_button.setText(self.color_value)
            self.color_button.setStyleSheet(f"background:{self.color_value};")

    def assign_texture(self):
        obj = self.selected()
        if not obj:
            return
        path, _ = QFileDialog.getOpenFileName(self, "Assign texture", "", "Images (*.png *.jpg *.jpeg *.bmp *.tga);;All files (*)")
        if path:
            obj.material["texture"] = path
            self.texture_label.setText(path)
            self.mark_dirty()
            self.log(f"Assigned texture reference: {path}")

    def clear_texture(self):
        obj = self.selected()
        if obj:
            obj.material["texture"] = ""
            self.texture_label.setText("No texture assigned")
            self.mark_dirty()

    def browse_mesh(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select mesh asset", "", "Mesh files (*.obj);;All files (*)")
        if path:
            self.mesh_edit.setText(path)

    def import_obj(self):
        path, _ = QFileDialog.getOpenFileName(self, "Import Wavefront OBJ", "", "Wavefront OBJ (*.obj)")
        if not path:
            return
        vertices = faces = 0
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as stream:
                for line in stream:
                    if line.startswith("v "):
                        vertices += 1
                    elif line.startswith("f "):
                        faces += 1
            if vertices == 0 or faces == 0:
                raise ValueError("OBJ must contain at least one vertex and one face")
        except (OSError, ValueError) as exc:
            QMessageBox.critical(self, "OBJ import failed", str(exc))
            return
        obj = self.doc.add(pathlib.Path(path).stem, "mesh", path)
        self.selected_id = obj.id
        self.refresh(obj.id)
        self.log(f"Imported OBJ reference: {path} ({vertices} vertices, {faces} faces)")

    def maybe_save(self):
        if not self.doc.dirty:
            return True
        answer = QMessageBox.question(self, "Unsaved changes", "Save changes before continuing?",
                                      QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard |
                                      QMessageBox.StandardButton.Cancel)
        if answer == QMessageBox.StandardButton.Cancel:
            return False
        if answer == QMessageBox.StandardButton.Save:
            return self.save_scene()
        return True

    def new_scene(self):
        if not self.maybe_save():
            return
        self.doc.new()
        self.selected_id = None
        self.refresh()
        self.log("Created new scene")

    def open_scene(self):
        if not self.maybe_save():
            return
        path, _ = QFileDialog.getOpenFileName(self, "Open FX3 scene", "", "FX3 Scene (*.fx3scene);;JSON (*.json)")
        if not path:
            return
        try:
            self.doc.load(path)
            self.selected_id = None
            self.refresh()
            self.log(f"Opened {path}")
        except Exception as exc:
            QMessageBox.critical(self, "Open failed", str(exc))

    def save_scene(self, save_as=False):
        path = self.doc.path
        if save_as or not path:
            path, _ = QFileDialog.getSaveFileName(self, "Save FX3 scene", path or "Scene.fx3scene",
                                                  "FX3 Scene (*.fx3scene)")
            if not path:
                return False
            if not path.lower().endswith(".fx3scene"):
                path += ".fx3scene"
        try:
            self.doc.save(path)
            self.update_title()
            self.log(f"Saved {path}")
            return True
        except Exception as exc:
            QMessageBox.critical(self, "Save failed", str(exc))
            return False

    def closeEvent(self, event: QCloseEvent):
        if self.maybe_save():
            event.accept()
        else:
            event.ignore()


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("FX3 PySide6 Editor")
    window = FX3Editor()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""FX3 Scene & Asset Editor — Tkinter, Python 3.10+, standard library only.

Edits the shared FX3Scene v1 JSON format. Includes scene CRUD, hierarchy,
transform/material inspector, OBJ mesh import, texture assignment and console.
"""
from __future__ import annotations

import json
import math
import os
import pathlib
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, colorchooser
from dataclasses import dataclass, asdict, field
from typing import Any

FORMAT = "FX3Scene"
VERSION = 1


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
        "base_color": "#b8c8df", "metallic": 0.0, "roughness": 0.5,
        "texture": ""
    })


class SceneDocument:
    def __init__(self) -> None:
        self.objects: list[SceneObject] = []
        self.path = ""
        self.dirty = False
        self.new()

    def new(self) -> None:
        self.objects = [
            SceneObject("camera-main", "Main Camera", "camera"),
            SceneObject("light-key", "Key Light", "directional_light"),
        ]
        self.path = ""
        self.dirty = False

    def add(self, name: str, kind: str = "mesh", mesh: str = "") -> SceneObject:
        import uuid
        obj = SceneObject(str(uuid.uuid4()), name, kind, mesh=mesh)
        self.objects.append(obj)
        self.dirty = True
        return obj

    def remove(self, object_id: str) -> None:
        self.objects = [o for o in self.objects if o.id != object_id]
        self.dirty = True

    def load(self, path: str) -> None:
        data = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
        if data.get("format") != FORMAT or data.get("version") != VERSION:
            raise ValueError("Unsupported scene format or version.")
        loaded = []
        seen = set()
        for item in data.get("objects", []):
            oid = str(item["id"])
            if not oid or oid in seen:
                raise ValueError("Object IDs must be unique and non-empty.")
            seen.add(oid)
            obj = SceneObject(
                id=oid, name=str(item["name"]), type=str(item["type"]),
                position=self._vec(item["position"], "position"),
                rotation=self._vec(item["rotation"], "rotation"),
                scale=self._vec(item["scale"], "scale"),
                mesh=str(item.get("mesh", "")),
                material=item.get("material", {}) or {}
            )
            loaded.append(obj)
        self.objects, self.path, self.dirty = loaded, path, False

    @staticmethod
    def _vec(value: Any, field_name: str) -> list[float]:
        if not isinstance(value, list) or len(value) != 3:
            raise ValueError(f"{field_name} must contain exactly three numbers.")
        result = [float(v) for v in value]
        if not all(math.isfinite(v) for v in result):
            raise ValueError(f"{field_name} contains a non-finite number.")
        return result

    def save(self, path: str | None = None) -> None:
        target = path or self.path
        if not target:
            raise ValueError("Choose a destination file first.")
        payload = {
            "format": FORMAT, "version": VERSION,
            "objects": [asdict(obj) for obj in self.objects]
        }
        target_path = pathlib.Path(target)
        target_path.parent.mkdir(parents=True, exist_ok=True)
        temp = target_path.with_suffix(target_path.suffix + ".tmp")
        temp.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
                        encoding="utf-8")
        os.replace(temp, target_path)
        self.path, self.dirty = str(target_path), False


class FX3Editor(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.doc = SceneDocument()
        self.selected_id: str | None = None
        self._vars: dict[str, tk.Variable] = {}
        self.title("FX3 Scene Editor 0.4.1")
        self.geometry("1320x850")
        self.minsize(980, 640)
        self._style()
        self._menu()
        self._layout()
        self.refresh_tree()
        self.log("FX3 Tkinter Editor ready — FX3Scene v1.")
        self.protocol("WM_DELETE_WINDOW", self.on_exit)
        self.bind("<Control-n>", lambda _e: self.new_scene())
        self.bind("<Control-o>", lambda _e: self.open_scene())
        self.bind("<Control-s>", lambda _e: self.save_scene())
        self.bind("<Delete>", lambda _e: self.delete_selected())

    def _style(self) -> None:
        style = ttk.Style(self)
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("Treeview", rowheight=25)
        style.configure("Title.TLabel", font=("Segoe UI", 12, "bold"))
        style.configure("Section.TLabelframe.Label", font=("Segoe UI", 10, "bold"))

    def _menu(self) -> None:
        root = tk.Menu(self)
        file_menu = tk.Menu(root, tearoff=False)
        file_menu.add_command(label="New Scene", accelerator="Ctrl+N", command=self.new_scene)
        file_menu.add_command(label="Open Scene…", accelerator="Ctrl+O", command=self.open_scene)
        file_menu.add_command(label="Save", accelerator="Ctrl+S", command=self.save_scene)
        file_menu.add_command(label="Save As…", command=lambda: self.save_scene(save_as=True))
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.on_exit)
        root.add_cascade(label="File", menu=file_menu)
        scene = tk.Menu(root, tearoff=False)
        scene.add_command(label="Add Empty Object", command=lambda: self.add_object("Empty", "empty"))
        scene.add_command(label="Add Mesh", command=lambda: self.add_object("Mesh", "mesh"))
        scene.add_command(label="Import Wavefront OBJ…", command=self.import_obj)
        scene.add_command(label="Delete Selected", command=self.delete_selected)
        root.add_cascade(label="Scene", menu=scene)
        assets = tk.Menu(root, tearoff=False)
        assets.add_command(label="Assign Texture to Selected…", command=self.assign_texture)
        assets.add_command(label="Clear Texture", command=self.clear_texture)
        root.add_cascade(label="Assets", menu=assets)
        help_menu = tk.Menu(root, tearoff=False)
        help_menu.add_command(label="About", command=lambda: messagebox.showinfo(
            "About FX3 Editor", "FX3 Scene Editor 0.4.1\nTkinter / Python 3\nFX3Scene JSON v1"))
        root.add_cascade(label="Help", menu=help_menu)
        self.config(menu=root)

    def _layout(self) -> None:
        toolbar = ttk.Frame(self, padding=6)
        toolbar.pack(fill="x")
        for label, command in [
            ("New", self.new_scene), ("Open", self.open_scene), ("Save", self.save_scene),
            ("+ Empty", lambda: self.add_object("Empty", "empty")),
            ("+ Mesh", lambda: self.add_object("Mesh", "mesh")),
            ("Import OBJ", self.import_obj), ("Delete", self.delete_selected),
        ]:
            ttk.Button(toolbar, text=label, command=command).pack(side="left", padx=3)
        self.path_label = ttk.Label(toolbar, text="Untitled", anchor="e")
        self.path_label.pack(side="right", fill="x", expand=True, padx=8)

        panes = ttk.Panedwindow(self, orient="horizontal")
        panes.pack(fill="both", expand=True, padx=6, pady=(0, 6))
        left = ttk.Frame(panes, padding=5)
        center = ttk.Frame(panes, padding=5)
        right = ttk.Frame(panes, padding=8)
        panes.add(left, weight=1)
        panes.add(center, weight=3)
        panes.add(right, weight=2)

        ttk.Label(left, text="SCENE OUTLINER", style="Title.TLabel").pack(anchor="w", pady=(2, 8))
        self.tree = ttk.Treeview(left, columns=("type",), show="tree headings", selectmode="browse")
        self.tree.heading("#0", text="Object")
        self.tree.heading("type", text="Type")
        self.tree.column("#0", width=180)
        self.tree.column("type", width=100, stretch=False)
        self.tree.pack(fill="both", expand=True)
        self.tree.bind("<<TreeviewSelect>>", self.on_select)
        ttk.Label(left, text="Drag-free hierarchy • select an item to edit",
                  wraplength=250).pack(anchor="w", pady=6)

        ttk.Label(center, text="VIEWPORT / ASSET PREVIEW", style="Title.TLabel").pack(anchor="w")
        self.viewport = tk.Canvas(center, background="#171d27", highlightthickness=0)
        self.viewport.pack(fill="both", expand=True, pady=6)
        self.viewport.bind("<Configure>", lambda _e: self.draw_viewport())
        self.viewport.bind("<Button-1>", lambda _e: self.log("Viewport preview: select objects in the Outliner."))
        ttk.Label(center, text="Scene-document preview (not a live DX12 render target)",
                  foreground="#7d8796").pack(anchor="w")

        ttk.Label(right, text="INSPECTOR", style="Title.TLabel").pack(anchor="w", pady=(2, 8))
        self.inspector = ttk.Frame(right)
        self.inspector.pack(fill="both", expand=True)
        self._build_inspector()

        console_frame = ttk.LabelFrame(self, text="Console", padding=5)
        console_frame.pack(fill="x", padx=6, pady=(0, 6))
        self.console = tk.Text(console_frame, height=6, wrap="word", state="disabled",
                               background="#111820", foreground="#c9d5e5",
                               insertbackground="white", font=("Consolas", 9))
        self.console.pack(fill="x")
        self.status = ttk.Label(self, text="Ready", relief="sunken", anchor="w", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _build_inspector(self) -> None:
        for child in self.inspector.winfo_children():
            child.destroy()
        self._vars.clear()
        self.name_var = tk.StringVar()
        self.type_var = tk.StringVar()
        self.mesh_var = tk.StringVar()
        self.color_var = tk.StringVar(value="#b8c8df")
        self.metal_var = tk.DoubleVar(value=0.0)
        self.rough_var = tk.DoubleVar(value=0.5)
        ttk.Label(self.inspector, text="Object").pack(anchor="w")
        self._entry("Name", self.name_var, 0)
        ttk.Label(self.inspector, text="Type", foreground="#65758b").pack(anchor="w", pady=(5, 0))
        ttk.Label(self.inspector, textvariable=self.type_var).pack(anchor="w")
        ttk.Label(self.inspector, text="Transform", style="Section.TLabelframe.Label").pack(anchor="w", pady=(14, 4))
        self.transform_vars = []
        for group in ("Position", "Rotation", "Scale"):
            ttk.Label(self.inspector, text=group).pack(anchor="w", pady=(5, 0))
            row = ttk.Frame(self.inspector)
            row.pack(fill="x")
            for axis in ("X", "Y", "Z"):
                var = tk.StringVar()
                self.transform_vars.append(var)
                cell = ttk.Frame(row)
                cell.pack(side="left", fill="x", expand=True, padx=(0, 3))
                ttk.Label(cell, text=axis).pack(anchor="w")
                ttk.Entry(cell, textvariable=var, width=8).pack(fill="x")
        ttk.Button(self.inspector, text="Apply Transform", command=self.apply_transform).pack(anchor="e", pady=5)
        ttk.Separator(self.inspector).pack(fill="x", pady=10)
        ttk.Label(self.inspector, text="Material", style="Section.TLabelframe.Label").pack(anchor="w")
        color_row = ttk.Frame(self.inspector)
        color_row.pack(fill="x", pady=4)
        ttk.Label(color_row, text="Base color").pack(side="left")
        ttk.Button(color_row, text="Choose…", command=self.choose_color).pack(side="right")
        self.color_swatch = tk.Label(self.inspector, textvariable=self.color_var, relief="groove")
        self.color_swatch.pack(fill="x")
        self._scale("Metallic", self.metal_var)
        self._scale("Roughness", self.rough_var)
        self.texture_label = ttk.Label(self.inspector, text="Texture: none", wraplength=250)
        self.texture_label.pack(anchor="w", pady=5)
        ttk.Button(self.inspector, text="Assign Texture…", command=self.assign_texture).pack(fill="x")
        ttk.Button(self.inspector, text="Apply Material", command=self.apply_material).pack(fill="x", pady=5)
        self._set_inspector_enabled(False)

    def _entry(self, label: str, var: tk.StringVar, _unused: int) -> None:
        ttk.Label(self.inspector, text=label).pack(anchor="w")
        ttk.Entry(self.inspector, textvariable=var).pack(fill="x")

    def _scale(self, label: str, var: tk.DoubleVar) -> None:
        ttk.Label(self.inspector, text=label).pack(anchor="w", pady=(4, 0))
        ttk.Scale(self.inspector, from_=0.0, to=1.0, variable=var).pack(fill="x")

    def _set_inspector_enabled(self, enabled: bool) -> None:
        state = "normal" if enabled else "disabled"
        for child in self.inspector.winfo_children():
            try:
                child.configure(state=state)
            except tk.TclError:
                pass

    def selected(self) -> SceneObject | None:
        return next((o for o in self.doc.objects if o.id == self.selected_id), None)

    def refresh_tree(self) -> None:
        self.tree.delete(*self.tree.get_children())
        root = self.tree.insert("", "end", text="Scene", values=("root",), open=True)
        for obj in self.doc.objects:
            self.tree.insert(root, "end", iid=obj.id, text=obj.name, values=(obj.type,))
        self.update_title()
        self.draw_viewport()

    def update_title(self) -> None:
        name = self.doc.path or "Untitled"
        dirty = " *" if self.doc.dirty else ""
        self.title(f"FX3 Scene Editor 0.4.1 — {name}{dirty}")
        self.path_label.configure(text=name)
        self.status.configure(text=f"{len(self.doc.objects)} objects | {'Modified' if self.doc.dirty else 'Saved'}")

    def on_select(self, _event=None) -> None:
        selection = self.tree.selection()
        self.selected_id = selection[0] if selection and selection[0] != self.tree.get_children("")[0] else None
        obj = self.selected()
        if not obj:
            self._set_inspector_enabled(False)
            return
        self._set_inspector_enabled(True)
        self.name_var.set(obj.name)
        self.type_var.set(obj.type)
        values = obj.position + obj.rotation + obj.scale
        for var, value in zip(self.transform_vars, values):
            var.set(f"{value:g}")
        mat = obj.material
        self.color_var.set(mat.get("base_color", "#b8c8df"))
        self.metal_var.set(float(mat.get("metallic", 0.0)))
        self.rough_var.set(float(mat.get("roughness", 0.5)))
        texture = mat.get("texture", "")
        self.texture_label.configure(text=f"Texture: {texture or 'none'}")
        self.color_swatch.configure(background=self.color_var.get())
        self.draw_viewport()

    def add_object(self, name: str, kind: str) -> None:
        obj = self.doc.add(name, kind)
        self.selected_id = obj.id
        self.refresh_tree()
        self.tree.selection_set(obj.id)
        self.tree.see(obj.id)
        self.on_select()
        self.log(f"Created {kind}: {name}")

    def delete_selected(self) -> None:
        obj = self.selected()
        if not obj:
            return
        self.doc.remove(obj.id)
        self.selected_id = None
        self.refresh_tree()
        self._set_inspector_enabled(False)
        self.log(f"Deleted object: {obj.name}")

    def apply_transform(self) -> None:
        obj = self.selected()
        if not obj:
            return
        try:
            values = [float(v.get()) for v in self.transform_vars]
            if not all(math.isfinite(v) for v in values):
                raise ValueError
        except ValueError:
            messagebox.showerror("Invalid transform", "Enter nine finite numeric values.")
            return
        obj.position, obj.rotation, obj.scale = values[:3], values[3:6], values[6:9]
        self.doc.dirty = True
        self.update_title()
        self.draw_viewport()
        self.log(f"Updated transform: {obj.name}")

    def choose_color(self) -> None:
        chosen = colorchooser.askcolor(self.color_var.get(), parent=self)
        if chosen and chosen[1]:
            self.color_var.set(chosen[1])
            self.color_swatch.configure(background=chosen[1])

    def apply_material(self) -> None:
        obj = self.selected()
        if not obj:
            return
        obj.material.update({
            "base_color": self.color_var.get(),
            "metallic": float(self.metal_var.get()),
            "roughness": float(self.rough_var.get())
        })
        self.doc.dirty = True
        self.update_title()
        self.log(f"Applied material parameters to {obj.name}")

    def assign_texture(self) -> None:
        obj = self.selected()
        if not obj:
            return
        path = filedialog.askopenfilename(
            title="Assign texture", filetypes=[("Images", "*.png *.jpg *.jpeg *.bmp *.tga"), ("All files", "*.*")])
        if path:
            obj.material["texture"] = path
            self.texture_label.configure(text=f"Texture: {path}")
            self.doc.dirty = True
            self.update_title()
            self.log(f"Assigned texture reference: {path}")

    def clear_texture(self) -> None:
        obj = self.selected()
        if obj:
            obj.material["texture"] = ""
            self.texture_label.configure(text="Texture: none")
            self.doc.dirty = True
            self.update_title()

    def import_obj(self) -> None:
        path = filedialog.askopenfilename(title="Import Wavefront OBJ",
            filetypes=[("Wavefront OBJ", "*.obj"), ("All files", "*.*")])
        if not path:
            return
        vertices, faces = 0, 0
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as stream:
                for line in stream:
                    line = line.strip()
                    if line.startswith("v "):
                        parts = line.split()
                        if len(parts) >= 4:
                            [float(x) for x in parts[1:4]]
                            vertices += 1
                    elif line.startswith("f "):
                        count = len(line.split()) - 1
                        if count >= 3:
                            faces += count - 2
            if vertices == 0 or faces == 0:
                raise ValueError("OBJ contains no valid vertices or polygon faces.")
        except (OSError, ValueError) as exc:
            messagebox.showerror("OBJ import failed", str(exc))
            return
        obj = self.doc.add(pathlib.Path(path).stem, "mesh", mesh=path)
        self.selected_id = obj.id
        self.refresh_tree()
        self.tree.selection_set(obj.id)
        self.on_select()
        self.log(f"Imported OBJ reference: {path} ({vertices} vertices, {faces} triangulated faces)")

    def draw_viewport(self) -> None:
        if not hasattr(self, "viewport"):
            return
        c = self.viewport
        c.delete("all")
        w, h = max(c.winfo_width(), 300), max(c.winfo_height(), 200)
        c.create_text(18, 16, anchor="nw", fill="#9eacc0",
                      text="SCENE DOCUMENT PREVIEW  •  FX3Scene v1", font=("Segoe UI", 9, "bold"))
        c.create_line(0, h * .72, w, h * .72, fill="#344253")
        c.create_line(w * .5, 42, w * .5, h, fill="#344253", dash=(3, 5))
        c.create_oval(w*.5-65, h*.42-65, w*.5+65, h*.42+65, outline="#52657b", width=2)
        c.create_polygon(w*.5, h*.42-48, w*.5-52, h*.42+38, w*.5+52, h*.42+38,
                         fill="#526f8f", outline="#b5c8df", width=2)
        c.create_text(w*.5, h*.42+80, fill="#d9e4f2",
                      text=f"{len(self.doc.objects)} scene objects", font=("Segoe UI", 11))
        obj = self.selected()
        if obj:
            c.create_text(18, 48, anchor="nw", fill="#d9e4f2",
                          text=f"Selected: {obj.name}  |  {obj.type}", font=("Segoe UI", 10))
            c.create_text(18, 70, anchor="nw", fill="#91a5bd",
                          text=f"Position: {obj.position}", font=("Consolas", 9))

    def log(self, text: str) -> None:
        if not hasattr(self, "console"):
            return
        self.console.configure(state="normal")
        self.console.insert("end", text + "\n")
        self.console.see("end")
        self.console.configure(state="disabled")

    def confirm_discard(self) -> bool:
        if not self.doc.dirty:
            return True
        answer = messagebox.askyesnocancel("Unsaved scene", "Save changes before continuing?")
        if answer is None:
            return False
        return self.save_scene() if answer else True

    def new_scene(self) -> None:
        if self.confirm_discard():
            self.doc.new()
            self.selected_id = None
            self.refresh_tree()
            self.log("Created new scene.")

    def open_scene(self) -> None:
        if not self.confirm_discard():
            return
        path = filedialog.askopenfilename(title="Open FX3 scene",
            filetypes=[("FX3 Scene", "*.fx3scene"), ("JSON", "*.json")])
        if not path:
            return
        try:
            self.doc.load(path)
        except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
            messagebox.showerror("Open failed", str(exc))
            return
        self.selected_id = None
        self.refresh_tree()
        self.log(f"Opened scene: {path}")

    def save_scene(self, save_as: bool = False) -> bool:
        path = self.doc.path
        if save_as or not path:
            path = filedialog.asksaveasfilename(
                title="Save FX3 scene", defaultextension=".fx3scene",
                filetypes=[("FX3 Scene", "*.fx3scene"), ("JSON", "*.json")])
            if not path:
                return False
        try:
            self.doc.save(path)
        except (OSError, ValueError) as exc:
            messagebox.showerror("Save failed", str(exc))
            return False
        self.update_title()
        self.log(f"Saved scene: {path}")
        return True

    def on_exit(self) -> None:
        if self.confirm_discard():
            self.destroy()


if __name__ == "__main__":
    FX3Editor().mainloop()

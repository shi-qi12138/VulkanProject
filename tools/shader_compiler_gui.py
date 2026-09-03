"""A small Tkinter front-end for compiling GLSL shaders to SPIR-V."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk


STAGES = {
    "自动识别": "",
    "顶点着色器 (vertex)": "vert",
    "片元着色器 (fragment)": "frag",
    "计算着色器 (compute)": "comp",
    "几何着色器 (geometry)": "geom",
    "曲面细分控制 (tess control)": "tesc",
    "曲面细分求值 (tess evaluation)": "tese",
}

EXTENSION_STAGES = {
    ".vert": "vert", ".vs": "vert", ".ver": "vert",
    ".frag": "frag", ".fs": "frag", ".fag": "frag",
    ".comp": "comp", ".cs": "comp",
    ".geom": "geom", ".gs": "geom",
    ".tesc": "tesc", ".tese": "tese",
}

SHADER_PATTERNS = (
    ("Shader 文件", "*.vert *.frag *.comp *.geom *.tesc *.tese *.glsl *.vs *.fs *.cs *.gs *.ver *.fag"),
    ("所有文件", "*.*"),
)


def find_glslc() -> str | None:
    """Find glslc on PATH or in the Vulkan SDK configured for this machine."""
    executable = shutil.which("glslc")
    if executable:
        return executable
    sdk = os.environ.get("VULKAN_SDK")
    if sdk:
        candidate = Path(sdk) / "Bin" / "glslc.exe"
        if candidate.is_file():
            return str(candidate)
    return None


def normalized_output_path(folder: str, name: str) -> Path:
    clean_name = name.strip()
    if not clean_name:
        raise ValueError("请输入输出文件名。")
    output = Path(clean_name)
    if output.suffix.lower() != ".spv":
        output = output.with_name(output.name + ".spv")
    return output if output.is_absolute() else Path(folder) / output


class ShaderCompilerApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("Shader → SPIR-V 编译工具")
        self.geometry("760x500")
        self.minsize(680, 440)

        default_folder = Path(__file__).resolve().parents[1] / "resources" / "shader"
        self.folder_var = tk.StringVar(value=str(default_folder))
        self.output_folder_var = tk.StringVar(value=str(default_folder))
        self.file_var = tk.StringVar()
        self.output_var = tk.StringVar(value="shader.spv")
        self.stage_var = tk.StringVar(value="自动识别")
        self.compiler_var = tk.StringVar(value=find_glslc() or "")
        self.status_var = tk.StringVar(value="就绪")

        self._build_ui()

    def _build_ui(self) -> None:
        root = ttk.Frame(self, padding=16)
        root.pack(fill=tk.BOTH, expand=True)
        root.columnconfigure(1, weight=1)
        root.rowconfigure(7, weight=1)

        ttk.Label(root, text="Shader 所在文件夹").grid(row=0, column=0, sticky="w", pady=6)
        ttk.Entry(root, textvariable=self.folder_var).grid(row=0, column=1, sticky="ew", padx=8)
        ttk.Button(root, text="选择文件夹…", command=self.choose_folder).grid(row=0, column=2)

        ttk.Label(root, text="Shader 文件").grid(row=1, column=0, sticky="w", pady=6)
        ttk.Entry(root, textvariable=self.file_var).grid(row=1, column=1, sticky="ew", padx=8)
        ttk.Button(root, text="选择文件…", command=self.choose_file).grid(row=1, column=2)

        ttk.Label(root, text="Shader 阶段").grid(row=2, column=0, sticky="w", pady=6)
        ttk.Combobox(root, textvariable=self.stage_var, values=list(STAGES), state="readonly").grid(
            row=2, column=1, sticky="ew", padx=8
        )

        ttk.Label(root, text="输出文件夹").grid(row=3, column=0, sticky="w", pady=6)
        ttk.Entry(root, textvariable=self.output_folder_var).grid(row=3, column=1, sticky="ew", padx=8)
        ttk.Button(root, text="选择文件夹…", command=self.choose_output_folder).grid(row=3, column=2)

        ttk.Label(root, text="输出文件名").grid(row=4, column=0, sticky="w", pady=6)
        ttk.Entry(root, textvariable=self.output_var).grid(row=4, column=1, sticky="ew", padx=8)
        ttk.Label(root, text="自动补全 .spv").grid(row=4, column=2, sticky="w")

        ttk.Label(root, text="glslc 编译器").grid(row=5, column=0, sticky="w", pady=6)
        ttk.Entry(root, textvariable=self.compiler_var).grid(row=5, column=1, sticky="ew", padx=8)
        ttk.Button(root, text="选择 glslc…", command=self.choose_compiler).grid(row=5, column=2)

        action = ttk.Frame(root)
        action.grid(row=6, column=0, columnspan=3, sticky="ew", pady=(12, 8))
        ttk.Button(action, text="编译为 SPIR-V", command=self.compile_shader).pack(side=tk.LEFT)
        ttk.Label(action, textvariable=self.status_var).pack(side=tk.LEFT, padx=14)

        self.log = tk.Text(root, height=12, wrap="word", state="disabled")
        self.log.grid(row=7, column=0, columnspan=3, sticky="nsew")
        scrollbar = ttk.Scrollbar(root, orient="vertical", command=self.log.yview)
        scrollbar.grid(row=7, column=3, sticky="ns")
        self.log.configure(yscrollcommand=scrollbar.set)

    def choose_folder(self) -> None:
        selected = filedialog.askdirectory(initialdir=self.folder_var.get() or os.getcwd())
        if selected:
            old_folder = self.folder_var.get()
            self.folder_var.set(selected)
            self.file_var.set("")
            if not self.output_folder_var.get().strip() or self.output_folder_var.get() == old_folder:
                self.output_folder_var.set(selected)

    def choose_output_folder(self) -> None:
        selected = filedialog.askdirectory(
            initialdir=self.output_folder_var.get() or self.folder_var.get() or os.getcwd()
        )
        if selected:
            self.output_folder_var.set(selected)

    def choose_file(self) -> None:
        selected = filedialog.askopenfilename(
            initialdir=self.folder_var.get() or os.getcwd(), filetypes=SHADER_PATTERNS
        )
        if selected:
            path = Path(selected)
            old_folder = self.folder_var.get()
            self.folder_var.set(str(path.parent))
            if not self.output_folder_var.get().strip() or self.output_folder_var.get() == old_folder:
                self.output_folder_var.set(str(path.parent))
            self.file_var.set(path.name)
            self.output_var.set(path.stem + ".spv")
            detected = EXTENSION_STAGES.get(path.suffix.lower())
            if detected:
                self.stage_var.set(next(label for label, stage in STAGES.items() if stage == detected))

    def choose_compiler(self) -> None:
        selected = filedialog.askopenfilename(
            title="选择 glslc 编译器", filetypes=(("glslc", "glslc.exe"), ("所有文件", "*.*"))
        )
        if selected:
            self.compiler_var.set(selected)

    def _write_log(self, content: str) -> None:
        self.log.configure(state="normal")
        self.log.delete("1.0", tk.END)
        self.log.insert(tk.END, content)
        self.log.configure(state="disabled")

    def compile_shader(self) -> None:
        try:
            compiler = Path(self.compiler_var.get().strip())
            folder = Path(self.folder_var.get().strip())
            output_folder_text = self.output_folder_var.get().strip()
            if not output_folder_text:
                raise ValueError("请选择有效的输出文件夹。")
            output_folder = Path(output_folder_text)
            source_text = self.file_var.get().strip()
            source = Path(source_text) if Path(source_text).is_absolute() else folder / source_text
            output = normalized_output_path(str(output_folder), self.output_var.get())

            if not compiler.is_file():
                raise ValueError("找不到 glslc，请安装 Vulkan SDK 或手动选择 glslc.exe。")
            if not source.is_file():
                raise ValueError("请选择有效的 Shader 文件。")
            output.parent.mkdir(parents=True, exist_ok=True)

            stage = STAGES[self.stage_var.get()] or EXTENSION_STAGES.get(source.suffix.lower(), "")
            command = [str(compiler), str(source), "-o", str(output)]
            if stage:
                # glslc must see the stage before an input with a custom extension.
                command.insert(1, f"-fshader-stage={stage}")

            self.status_var.set("正在编译…")
            self.update_idletasks()
            result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
            details = result.stdout + result.stderr
            self._write_log("> " + subprocess.list2cmdline(command) + "\n\n" + (details or "编译器没有输出信息。"))

            if result.returncode == 0:
                self.status_var.set("编译成功")
                messagebox.showinfo("编译成功", f"已生成：\n{output}")
            else:
                self.status_var.set("编译失败")
                messagebox.showerror("编译失败", "请查看窗口下方的编译器输出。")
        except (OSError, ValueError, KeyError) as exc:
            self.status_var.set("无法编译")
            messagebox.showerror("错误", str(exc))


def main() -> None:
    if "--help" in sys.argv or "-h" in sys.argv:
        print("启动图形界面：python shader_compiler_gui.py")
        return
    ShaderCompilerApp().mainloop()


if __name__ == "__main__":
    main()

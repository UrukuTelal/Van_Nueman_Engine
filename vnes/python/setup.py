import sys
from setuptools import setup, Extension
import pybind11

# C++ extension module exposing VNES library
vnes_cpp = Extension(
    "vnes_cpp",
    sources=["src/bindings.cpp", "../vnes_library.cpp"],
    include_dirs=[
        pybind11.get_include(),
        pybind11.get_include(user=True),
        "../../",      # Van_Nueman_Engine root (for kernels/voxel_svo.h)
        "../../include",
        "../../vn",
        "../../generated",
    ],
    language="c++",
    extra_compile_args=["/std:c++17", "/O2"] if sys.platform == "win32"
                       else ["-std=c++17", "-O2", "-fvisibility=hidden"],
)

setup(
    ext_modules=[vnes_cpp],
    packages=["vnes"],
    package_dir={"vnes": "vnes"},
    zip_safe=False,
)

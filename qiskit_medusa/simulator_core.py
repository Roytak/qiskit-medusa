import platform
import pathlib
import ctypes

lib_ext = "so" if platform.system() == "Linux" else "dylib"
lib_path = pathlib.Path(__file__).parent / f"libmedusa.{lib_ext}"
lib = ctypes.CDLL(str(lib_path))

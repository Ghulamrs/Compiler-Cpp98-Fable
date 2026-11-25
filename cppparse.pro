{
  "name": "cppparse",
  "toolchain": "cpp11",
  "arch": "x86_64-windows",
  "include": [
    "cppparse/driver",
    "cppparse/common",
    "cppparse/frontend",
    "cppparse/middle",
    "cppparse/backend",
    "cppparse/eh",
    "cppparse/cv",
    "cppparse/target",
    "cppparse/analyzer",
    "cppparse/analyzer/report"
  ],
  "groups": {
    "driver": ["cppparse/driver/*.cpp"],
    "common": ["cppparse/common/*.cpp"],
    "frontend": ["cppparse/frontend/*.cpp"],
    "middle": ["cppparse/middle/*.cpp"],
    "backend": ["cppparse/backend/*.cpp"],
    "eh": ["cppparse/eh/*.cpp"],
    "cv": ["cppparse/cv/*.cpp"],
    "target": ["cppparse/target/*.cpp"],
    "analyzer": ["cppparse/analyzer/*.cpp"],
    "report": ["cppparse/analyzer/report/*.cpp"],
    "headers": [
      "cppparse/analyzer/*.hpp",
      "cppparse/analyzer/report/*.hpp",
      "cppparse/backend/*.hpp",
      "cppparse/common/*.hpp",
      "cppparse/cv/*.hpp",
      "cppparse/eh/*.hpp",
      "cppparse/target/*.hpp",
      "cppparse/frontend/*.hpp",
      "cppparse/middle/*.hpp"
    ],
    "Input": ["sample.cpp", "VectorTest.cpp", "Vector.hpp", "MatrixTest.cpp", "Matrix.hpp"],
    "include": ["include/*"],
    "platform": ["platform/*.h", "platform/sys/*.h"],
    "libcpp": ["libcpp/*.cpp"],
    "runtime": ["runtime/*.cpp"],
    "docs": ["README.md", "docs/*.md", "docs/*.txt", "crc32.txt"],
    "sandbox": ["sandbox/*.ps1", "sandbox/scripts/*.ps1", "sandbox/scripts/*.py"]
  },
  "build": {
    "target": "cppparse",
    "groups": ["driver", "common", "frontend", "middle", "backend", "eh", "cv", "target", "analyzer", "report"],
    "args": ["--stdout", "sample.cpp"]
  }
}

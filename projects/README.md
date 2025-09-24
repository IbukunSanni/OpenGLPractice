# OpenGL Projects

This directory contains different OpenGL experiments and projects.

## Structure

- `triangle-animation/` - Pulsing triangle animation with color interpolation
  - Contains the animated triangle that scales with sine wave
  - Uses the original triangle mesh with inner vertices
  
- `new-experiment/` - Template for new OpenGL experiments
  - Basic setup with TODO comments
  - Ready to customize for new projects

## How to Use

1. **Working on the triangle animation**: 
   - Navigate to `projects/triangle-animation/`
   - The main code is in `main.cpp`
   - Shader files are included

2. **Starting a new experiment**:
   - Copy the `new-experiment` folder or use it as reference
   - Create a new folder like `projects/my-new-project/`
   - Customize the template as needed

3. **Shared Resources**:
   - All projects can use the shared class files in the parent directory:
     - `shaderClass.h/.cpp`
     - `VAO.h/.cpp`, `VBO.h/.cpp`, `EBO.h/.cpp`
     - `glad.c` and other libraries

## Building

The main Visual Studio project file (`OpenGLPractice.vcxproj`) can be updated to point to different main.cpp files as needed.
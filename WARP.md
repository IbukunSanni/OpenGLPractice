# WARP AI Rules for OpenGL Project

This file contains specific rules and preferences for AI assistance in this OpenGL project.

## Git & Version Control Rules

### Commit Messages
- **ALWAYS limit commit messages to 1 line maximum**
- Use conventional commit format: `type: description`
- Examples:
  - `feat: add triangle animation project`
  - `fix: resolve shader compilation error`
  - `chore: update .gitignore for build artifacts`
  - `docs: add project structure documentation`

### Conventional Commit Types
- `feat:` - New features or experiments
- `fix:` - Bug fixes
- `chore:` - Maintenance tasks (gitignore, build config)
- `docs:` - Documentation updates
- `refactor:` - Code restructuring without feature changes
- `perf:` - Performance improvements
- `test:` - Adding or modifying tests

## Project Structure Rules

### Multi-Project Organization
- Keep all OpenGL experiments in `projects/` subdirectories
- Name projects with descriptive folders: `00_Window/`, `01_Triangle/`, etc.
- Each project should have its own `main.cpp` file
- Share common classes (VAO, VBO, EBO, Shader) at root level

### Build Configuration
- Use Visual Studio build configurations for different projects:
  - `Debug` - Original Main.cpp
  - `TriangleAnimation` - projects/triangle-animation/main.cpp
  - `NewExperiment` - projects/new-experiment/main.cpp
- Add new configurations as needed for new projects

## Code Style Rules

### OpenGL Conventions
- Follow existing naming conventions in the codebase
- Use descriptive variable names for OpenGL objects (VAO1, VBO1, etc.)
- Keep shader files (.vert, .frag) alongside their respective projects
- Use consistent indentation and formatting

### File Organization
- Main executable code: `main.cpp` in project folders
- Shared classes: Root level (.h/.cpp pairs)
- Resources: Keep shaders close to the code that uses them
- Build artifacts: Automatically ignored by .gitignore

## Development Workflow Rules

### When Creating New Projects
1. Copy the `new-experiment` template or create from scratch
2. Add shader files if needed
3. Update Visual Studio project configurations if desired
4. Test build and run before committing
5. Commit with conventional commit format (1 line)

### When Making Changes
- Test changes locally before committing
- Use descriptive but concise commit messages
- Keep commits focused on single logical changes
- Update documentation when adding new features

## AI Assistant Preferences

### Communication Style
- Be direct and concise in explanations
- Provide working code examples when relevant
- Focus on practical solutions over theoretical discussions

### Code Generation
- Generate complete, working code blocks
- Include necessary headers and dependencies
- Follow existing project patterns and conventions
- Test suggestions when possible

### Problem Solving
- Prioritize getting things working first
- Suggest incremental improvements
- Explain complex OpenGL concepts when needed
- Provide multiple approaches when relevant

---

*This file helps ensure consistent development practices and AI assistance for the OpenGL learning project.*
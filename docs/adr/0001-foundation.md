# ADR 0001: M0 foundation

Status: Accepted for initial implementation.

Use C++20, Ninja CMake presets, and separate static Core and Platform targets.
Runtime consumes Platform; Core has no dependency on Platform or the application.
Only M0 dependencies are fetched, pinned to immutable upstream commits:
GLFW 3.4, spdlog 1.15.3, and Tracy 0.12.2. Other modules and libraries arrive
in their assigned milestones, rather than creating empty targets now.

The registry owns modules and borrows an application-owned EngineContext. It
validates the complete dependency graph before callbacks, registers in dependency
order, starts in dependency order, and shuts down in reverse order. Startup is
one-shot; recreate the registry to retry. Registration must be resource-free.
Shutdown must be safe after a partially failed startup.

Deviation from the illustrative IModule signature: registration and startup return
Result instead of void, to support explicit failures without exceptions across
module boundaries. Lifecycle callbacks are noexcept; module implementations must
catch internal third-party exceptions and translate them to Result. Resource
destructors and shutdown callbacks must not throw.

GLFW owns process windowing initialization inside PlatformModule. Windows are
RAII values destroyed before Platform shutdown. GLFW is private to Platform;
the future surface interface is deferred to M1. Windows use GLFW_NO_API now,
so no OpenGL context or renderer is introduced. Escape closes the window.

Tracy scopes record the runtime loop and frame markers. On-demand capture avoids
unbounded recording when no profiler connects. Logging is the explicitly permitted
process-global exception; other services will be explicit context references.

M0 does not draw into the window. A blank client area is expected until M1.

#pragma once
namespace Engine {
// Application-owned context, accessed on the main thread. Modules borrow it.
// Add typed service references here as their owning layers are introduced.
struct EngineContext {};
}

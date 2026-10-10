#pragma once
#include "engine/scene/Commands.hpp"
#include "engine/assets/Registry.hpp"
// Runtime fixture edits through commands; main-thread only.
Engine::Result BuildDemoScene(Engine::Scene::World& world,Engine::Scene::CommandHistory& history);
Engine::Result BuildImportedScene(const Engine::Assets::ImportedScene& scene,Engine::Scene::World& world,Engine::Scene::CommandHistory& history);

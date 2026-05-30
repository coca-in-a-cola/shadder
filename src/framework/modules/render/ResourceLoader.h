#pragma once

#include <d3d11.h>

class World;

// -----------------------------------------------------------------------------
// ResourceLoader — создаёт GPU-ресурсы из описаний в Mesh/MaterialComponent.
//
// Идея (ECS-way): пользователь задаёт сущности только ОПИСАНИЕ ресурса
// (примитив/размер/цвет в MeshComponent, путь к шейдеру в MaterialComponent),
// а фреймворк один раз заливает их в GPU перед стартом игрового цикла.
//
// Использование:
//     ResourceLoader::UploadAll(world, game.GetDevice());  // перед game.Run()
//
// Повторный вызов безопасен: уже загруженные ресурсы (uploaded == true)
// пропускаются. Скомпилированные шейдеры кешируются по пути, поэтому несколько
// сущностей с одним .hlsl делят один VS/PS/InputLayout (одна компиляция).
// -----------------------------------------------------------------------------
class ResourceLoader {
public:
    // Заливает все ещё не загруженные Mesh/Material-описания в GPU.
    static void UploadAll(World& world, ID3D11Device* device);
};

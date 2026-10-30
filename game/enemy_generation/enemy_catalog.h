#pragma once

#include "../characters/enemy.h"

#include <vector>

class EnemyCatalog
{
public:
    EnemyCatalog();
    explicit EnemyCatalog(std::vector<EnemyDefinition> definitions);

    [[nodiscard]] const std::vector<EnemyDefinition> &definitions() const noexcept;

private:
    std::vector<EnemyDefinition> _definitions;
};

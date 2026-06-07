#pragma once

#include <cstdint>
#include <string>



enum class DamageType
{
    Physical, // Unit Physical Damage ignores Armor
    Magic,    // Unit Magic Damage ignores Magic Resist
    TrueDamage // Unit True Damage ignores Armor and Magic Resist
};


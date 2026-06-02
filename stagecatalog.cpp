#include "stagecatalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QPoint>

QStringList fallbackLevelPaths()
{
    return {
        QDir(QCoreApplication::applicationDirPath()).filePath("levels"),
        QDir::current().filePath("levels"),
        QCoreApplication::applicationDirPath(),
        QDir::currentPath()
    };
}

QVector<StageCard> stageCatalog()
{
    return {
        {0, false, "Stage 1", "Pointer Gate", "Warm-up path. Learn movement, chests, and code blocks."},
        {1, false, "Stage 2", "Stack Corridor", "A compact dungeon route with stricter block choices."},
        {2, false, "Stage 3", "Array Depths", "Beware the bounds. The boss gate starts watching."},
        {3, false, "Stage 4", "Heap Bridge", "A branching route that asks the player to plan block order."},
        {4, false, "Stage 5", "Lambda Atrium", "Introduce slightly longer fill chains and optional clues."},
        {5, false, "Stage 6", "Graph Lift", "Unlocked after the lower bridge is cleared."},
        {6, false, "Stage 7", "Bit Forge", "A late normal route for denser C++ syntax puzzles."},
        {7, false, "Stage 8", "Memory Crown", "The final normal ascent before the spire core."},
        {8, false, "Stage 9", "Compile Throne", "Normal boss placeholder for the full chapter arc."},
        {9, true, "EX-1", "Template Rift", "Extra challenge route. Designed for later hard levels."},
        {10, true, "EX-2", "Iterator Vault", "Optional branch with denser clue and chest logic."},
        {11, true, "EX-3", "Undefined Core", "A harder fill route with fewer visible hints."},
        {12, true, "EX-4", "Const Labyrinth", "Requires prior EX proof before entry."},
        {13, true, "EX-5", "Overload Gate", "A special branch for advanced C++ mechanics."},
        {14, true, "EX-6", "Shadow Linker", "A locked extension for future JSON content."},
        {15, true, "EX-7", "Metaprogram Pit", "Late EX placeholder with strict topology."},
        {16, true, "EX-8", "Undefined Peak", "The final optional climb."},
        {17, true, "EX-9", "Core Dump", "EX boss placeholder for the chapter finale."}
    };
}

LevelData createPreviewLevel()
{
    LevelData level;
    level.mapWidth = 7;
    level.mapHeight = 6;
    level.bagSize = 5;
    level.levelType = "preview";
    level.startpos = QPoint(1, 1);
    level.endText = "Preview boss cleared.";
    level.specialTags = {"preview_map"};
    level.mapGrid = {
        {"#", "#", "#", "#", "#", "#", "#"},
        {"#", "start", ".", "chest_1", ".", "monster_1", "#"},
        {"#", ".", "#", ".", "#", ".", "#"},
        {"#", "clue_1", ".", ".", ".", "monster_2", "#"},
        {"#", ".", "chest_2", "#", ".", "boss", "#"},
        {"#", "#", "#", "#", "#", "#", "#"}
    };

    Clue clue;
    clue.pos = QPoint(3, 1);
    clue.val = "cout << *func1(x);";
    level.clues["clue_1"] = clue;

    Chest chest1;
    chest1.pos = QPoint(1, 3);
    chest1.chestId = "chest_1";
    chest1.forcedPick = true;
    chest1.repeat = true;
    chest1.blocks["block_add1"] = CodeBlock{"block_add1", "a += 1;"};
    chest1.blocks["block_mul2"] = CodeBlock{"block_mul2", "a *= 2;"};
    level.chests[chest1.chestId] = chest1;

    Chest chest2;
    chest2.pos = QPoint(4, 2);
    chest2.chestId = "chest_2";
    chest2.forcedPick = false;
    chest2.repeat = false;
    chest2.blocks["block_ret_val"] = CodeBlock{"block_ret_val", "return a;"};
    chest2.blocks["block_ret_loc"] = CodeBlock{"block_ret_loc", "return &a;"};
    level.chests[chest2.chestId] = chest2;

    Monster monster1;
    monster1.pos = QPoint(1, 5);
    monster1.monsterId = "monster_1";
    monster1.name = "func_ret";
    monster1.nickname = "Runtime Bug";
    monster1.type = "function";
    monster1.codeTemplate = "int* func1(int& a){\n    a++;\n    $space1$\n}";
    monster1.spaces.append(Space{"space1", "regex", {"^block.*"}});
    level.monsters[monster1.monsterId] = monster1;

    Monster monster2;
    monster2.pos = QPoint(3, 5);
    monster2.monsterId = "monster_2";
    monster2.name = "class_calc";
    monster2.nickname = "Stack Trace";
    monster2.type = "class";
    monster2.codeTemplate = "class A{\npublic:\n    int x;\n    A(int a=0){\n        $space1$\n        $space2$\n        x=a;\n    }\n};";
    monster2.spaces.append(Space{"space1", "prefix", {"block"}});
    monster2.spaces.append(Space{"space2", "find", {"block"}});
    level.monsters[monster2.monsterId] = monster2;

    Boss boss;
    boss.pos = QPoint(4, 5);
    boss.monsterId = "boss";
    boss.name = "boss";
    boss.nickname = "Marry Ann";
    boss.type = "boss";
    boss.codeTemplate = "$space1$\n$space2$\nint main(){ return 0; }";
    boss.input = "1";
    boss.output = "5";
    boss.spaces.append(Space{"space1", "match", {"func_ret[block_ret_loc]"}});
    boss.spaces.append(Space{"space2", "match", {"class_calc[block_add1,block_mul2]"}});
    level.boss = boss;

    return level;
}

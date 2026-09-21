// 探针：规则增删对扫描结果的影响
//
// 复现用户报告的两个现象：
//   1) 移除了 .db 相关规则后，扫描结果里仍出现 .db 文件
//   2) 添加了 .exe 清理规则后，扫描结果里没有 .exe 文件
//
// 本探针不猜，直接测「规则怎么写才生效、删了是否真生效」：
//   内置规则里与 .db 有关的有【两条】：`.db/`（目录规则）与 `*.db`（文件规则）。
//   只删其中一条，另一条仍会命中 .db 文件。
//   而 `.exe` 若写成无通配的 `.exe`，只能匹配文件名恰为 ".exe" 的文件，不会匹配 xxx.exe。
//
// 用例：
//   A 内置规则基线：.db 文件是否命中
//   B 只移除 `*.db`（保留 `.db/`）：.db 文件是否仍命中  ← 对应现象 1
//   C 只移除 `.db/`（保留 `*.db`）：.db 文件是否仍命中  ← 对应现象 1
//   D 添加 `*.exe`：.exe 文件是否命中（正确写法）
//   E 添加 `.exe`（无通配）：.exe 文件是否命中  ← 对应现象 2
//   F 移除内置 `*.obj` 后：.obj 文件不再命中（验证移除确实生效）
//
// 依赖：Qt5::Core

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <iostream>

#include "rules/RuleEngine.h"

static int g_passCount = 0;
static int g_failCount = 0;

static void Check(bool condition, const QString& description)
{
    if (condition)
    {
        std::cout << "[PASS] " << description.toStdString() << std::endl;
        ++g_passCount;
    }
    else
    {
        std::cout << "[FAIL] " << description.toStdString() << std::endl;
        ++g_failCount;
    }
}

static bool MakeFile(const QString& fullPath)
{
    QFile f(fullPath);
    if (!f.open(QIODevice::WriteOnly))
    {
        return false;
    }
    f.write("x", 1);
    f.close();
    return true;
}

// 在引擎的清理规则中查找指定模式，返回其全局索引；找不到返回 -1
static int FindCleanRule(const RuleEngine& engine, const QString& pattern)
{
    const auto rules = engine.GetRules();
    for (int i = 0; i < rules.size(); ++i)
    {
        if (rules[i].type == RuleType::Clean && rules[i].pattern == pattern)
        {
            return i;
        }
    }
    return -1;
}

// 统计引擎中清理规则里与 .db 相关的模式（用于呈现"两条 .db 规则"这一事实）
static QStringList DbPatterns(const RuleEngine& engine)
{
    QStringList out;
    for (const auto& r : engine.GetRules())
    {
        if (r.type == RuleType::Clean && r.pattern.contains(".db"))
        {
            out << r.pattern;
        }
    }
    return out;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== 规则增删对扫描结果的影响探针 ===" << std::endl;
    std::cout << std::endl;

    QTemporaryDir tempDir;
    Check(tempDir.isValid(), "临时目录创建成功");
    const QString root = tempDir.path();
    const QString dbFile = root + "/codegraph.db";
    const QString objFile = root + "/a.obj";
    const QString exeFile = root + "/tool.exe";
    MakeFile(dbFile);
    MakeFile(objFile);
    MakeFile(exeFile);

    // ---- 先呈现事实：内置规则里有几条与 .db 有关 ----
    {
        RuleEngine engine;
        const QStringList db = DbPatterns(engine);
        std::cout << "       内置清理规则中与 .db 有关的共 " << db.size() << " 条: "
                  << db.join(" | ").toStdString() << std::endl;
        Check(db.size() == 2, "内置规则含两条 .db 相关规则（.db/ 目录规则 与 *.db 文件规则）");
    }

    // ---- A：基线 ----
    {
        RuleEngine engine;
        Check(engine.MatchFile(dbFile).isCleanTarget, "A: 内置规则下 .db 文件命中清理");
        Check(engine.MatchFile(objFile).isCleanTarget, "A: 内置规则下 .obj 文件命中清理");
    }

    // ---- B：只移除 *.db，保留 .db/ ----
    {
        RuleEngine engine;
        const int idx = FindCleanRule(engine, "*.db");
        Check(idx >= 0, "B: 找到内置 *.db 规则");
        engine.RemoveRule(idx);
        Check(FindCleanRule(engine, "*.db") < 0, "B: *.db 规则已移除");
        Check(FindCleanRule(engine, ".db/") >= 0, "B: .db/ 目录规则仍在");
        // .db/ 是目录规则，只匹配目录名，不参与文件匹配，
        // 故移除 *.db 后 .db 文件确实不再命中——移除本身是生效的
        Check(!engine.MatchFile(dbFile).isCleanTarget,
              "B: 仅移除 *.db 后，.db 文件不再命中（.db/ 为目录规则，不匹配文件）");
    }

    // ---- C：只移除 .db/，保留 *.db ----
    {
        RuleEngine engine;
        const int idx = FindCleanRule(engine, ".db/");
        Check(idx >= 0, "C: 找到内置 .db/ 规则");
        engine.RemoveRule(idx);
        Check(FindCleanRule(engine, ".db/") < 0, "C: .db/ 规则已移除");
        Check(engine.MatchFile(dbFile).isCleanTarget,
              "C: 仅移除 .db/ 后，.db 文件【仍被 *.db 命中】");
    }

    // ---- D：添加 *.exe（正确写法）----
    {
        RuleEngine engine;
        engine.AddCleanRule("*.exe");
        Check(engine.MatchFile(exeFile).isCleanTarget, "D: 添加 *.exe 后，tool.exe 命中清理");
    }

    // ---- E：添加 .exe（无通配）----
    {
        RuleEngine engine;
        engine.AddCleanRule(".exe");
        const bool hit = engine.MatchFile(exeFile).isCleanTarget;
        std::cout << "       添加 \".exe\"（无通配）后 tool.exe 是否命中: "
                  << (hit ? "命中" : "未命中") << std::endl;
        Check(!hit, "E: 无通配的 .exe 只匹配文件名恰为 \".exe\" 的文件，不匹配 tool.exe");
    }

    // ---- F：移除内置 *.obj 后确实不再命中（验证移除生效）----
    {
        RuleEngine engine;
        Check(engine.MatchFile(objFile).isCleanTarget, "F: 移除前 .obj 命中");
        const int idx = FindCleanRule(engine, "*.obj");
        Check(idx >= 0, "F: 找到内置 *.obj 规则");
        engine.RemoveRule(idx);
        Check(!engine.MatchFile(objFile).isCleanTarget, "F: 移除 *.obj 后 .obj 不再命中（移除生效）");
    }

    // ---- G：裸扩展名自动补全（NormalizePattern）----
    // .exe 与 .gitignore 形式相同（点 + 字母），必须能区分：
    //   .exe       → 补成 *.exe（用户意图是"这类扩展名"）
    //   .gitignore → 不补（它整体就是文件名，补成 *.gitignore 反而匹配不到）
    {
        RuleEngine engine;                       // 取内置保留规则用于判定完整文件名
        const QStringList keep = engine.KeepRules();

        Check(keep.contains(".gitignore"), "G: 内置保留规则含 .gitignore（作为完整文件名）");

        Check(RuleEngine::NormalizePattern(".exe", keep) == "*.exe",
              "G: .exe 补全为 *.exe");
        Check(RuleEngine::NormalizePattern(".db", keep) == "*.db",
              "G: .db 补全为 *.db");
        Check(RuleEngine::NormalizePattern(".TMP", keep) == "*.TMP",
              "G: 补全保留原大小写（仅补前缀）");

        Check(RuleEngine::NormalizePattern(".gitignore", keep) == ".gitignore",
              "G: .gitignore 作为完整文件名不做补全");
        Check(RuleEngine::NormalizePattern("*.obj", keep) == "*.obj",
              "G: 已含通配符的不动");
        Check(RuleEngine::NormalizePattern("build/", keep) == "build/",
              "G: 目录规则不动");
        Check(RuleEngine::NormalizePattern(".a.b", keep) == ".a.b",
              "G: 多段形式视为完整文件名，不动");
        Check(RuleEngine::NormalizePattern("main.cpp", keep) == "main.cpp",
              "G: 不以点开头的不动");
    }

    // ---- H：补全后的规则确实能匹配，未补全的匹配不到（端到端对照）----
    {
        RuleEngine engine;
        const QStringList keep = engine.KeepRules();

        RuleEngine raw;
        raw.AddCleanRule(".exe");
        Check(!raw.MatchFile(exeFile).isCleanTarget,
              "H: 未补全的裸 .exe 匹配不到 tool.exe（静默失效）");

        RuleEngine fixed;
        fixed.AddCleanRule(RuleEngine::NormalizePattern(".exe", keep));
        Check(fixed.MatchFile(exeFile).isCleanTarget,
              "H: 经补全后的规则能匹配 tool.exe");
    }

    // ---- I：编辑规则应保持位置不变（RuleEngine::ReplaceRule）----
    // 规则页编辑某条规则时若用「删除 + 追加」实现，该规则会被挪到列表末尾，
    // 静默改变其匹配优先级，且表格显示顺序与引擎实际顺序脱节。
    // 注意：RuleEngine 构造即加载 58 条内置规则，AddCleanRule 追加在其后，
    //       故自定义规则从 base 开始编号。
    {
        RuleEngine viaReplace;
        const int base = viaReplace.GetRules().size();          // 内置规则条数
        viaReplace.AddCleanRule("*.aaa");
        viaReplace.AddCleanRule("*.bbb");
        viaReplace.AddCleanRule("*.ccc");

        const auto beforeRules = viaReplace.GetRules();
        const int idxB = base + 1;                              // 自定义段中间那条
        Check(beforeRules[idxB].pattern == "*.bbb", "I: 替换前自定义段第 2 条为 *.bbb");

        viaReplace.ReplaceRule(idxB, "*.bbb2");
        const auto afterRules = viaReplace.GetRules();
        Check(afterRules[idxB].pattern == "*.bbb2", "I: 替换后原位即新模式串");
        Check(afterRules.size() == beforeRules.size(), "I: 替换不改变规则总数");
        Check(afterRules[base].pattern == "*.aaa" && afterRules[base + 2].pattern == "*.ccc",
              "I: 前后两条规则位置不受影响");

        // 对照：用「删除 + 追加」实现时，该规则会被挪到末尾
        RuleEngine viaRemoveAdd;
        const int base2 = viaRemoveAdd.GetRules().size();
        viaRemoveAdd.AddCleanRule("*.aaa");
        viaRemoveAdd.AddCleanRule("*.bbb");
        viaRemoveAdd.AddCleanRule("*.ccc");
        viaRemoveAdd.RemoveRule(base2 + 1);
        viaRemoveAdd.AddCleanRule("*.bbb2");
        const auto raRules = viaRemoveAdd.GetRules();
        Check(raRules[raRules.size() - 1].pattern == "*.bbb2",
              "I: 对照——删除+追加会把该规则挪到末尾（故不能这样实现编辑）");

        // 模式串清单需同步，避免与 m_rules 脱节
        Check(viaReplace.CleanRules().contains("*.bbb2"), "I: 替换后清理规则清单含新模式串");
        Check(!viaReplace.CleanRules().contains("*.bbb"), "I: 替换后清理规则清单不含旧模式串");
    }

    // ---- J：替换后目录规则标记随模式串重新判定 ----
    {
        RuleEngine engine;
        const int idx = engine.GetRules().size();               // 自定义段起点
        engine.AddCleanRule("*.obj");

        Check(!engine.GetRules()[idx].isDirRule, "J: *.obj 初始为非目录规则");
        engine.ReplaceRule(idx, "build/");
        Check(engine.GetRules()[idx].isDirRule, "J: 替换为 build/ 后变为目录规则");
        engine.ReplaceRule(idx, "*.tmp");
        Check(!engine.GetRules()[idx].isDirRule, "J: 再替换为 *.tmp 后恢复为非目录规则");
    }

    // ---- K：替换后匹配行为跟随新模式串 ----
    {
        // 用 .aaa 这个内置规则不覆盖的扩展名，才能验证"旧自定义模式已失效"
        const QString aaaFile = root + "/sample.aaa";
        MakeFile(aaaFile);

        RuleEngine engine;
        const int idx = engine.GetRules().size();
        engine.AddCleanRule("*.aaa");
        Check(engine.MatchFile(aaaFile).isCleanTarget, "K: 替换前 *.aaa 匹配 sample.aaa");
        Check(!engine.MatchFile(exeFile).isCleanTarget, "K: 替换前 *.aaa 不匹配 tool.exe");

        engine.ReplaceRule(idx, "*.exe");
        Check(engine.MatchFile(exeFile).isCleanTarget, "K: 替换为 *.exe 后匹配 tool.exe");
        Check(!engine.MatchFile(aaaFile).isCleanTarget, "K: 旧模式失效，sample.aaa 不再命中");
    }

    // ---- L：越界索引 / 空模式串为无操作 ----
    {
        RuleEngine engine;
        const int base = engine.GetRules().size();
        engine.AddCleanRule("*.aaa");
        const int n = engine.GetRules().size();

        Check(!engine.ReplaceRule(-1, "*.xxx"), "L: 负索引返回 false");
        Check(!engine.ReplaceRule(n, "*.xxx"), "L: 越界索引返回 false");
        Check(!engine.ReplaceRule(base, ""), "L: 空模式串返回 false");
        Check(engine.GetRules().size() == n, "L: 无效替换不改变规则总数");
        Check(engine.GetRules()[base].pattern == "*.aaa", "L: 无效替换不改变原模式串");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount == 0 ? 0 : 1;
}

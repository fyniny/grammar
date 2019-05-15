#include "grammar.h"
#include <set>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <queue>

static void extractEmpty(int startPos, NonTerminal Vn, std::string express, std::set<std::string> &expressSet);

// 获取可以推导出ε的非终结符集
// 注意: 此操作会删除原来产生式中能够直接推导出 A -> ε
std::set<NonTerminal> *Grammar::DerivateEmptyExpress()
{
    // 定义含有可能推导出ε的非终结符 集合
    std::set<NonTerminal> *derivateEmptyTerminalSet = new std::set<NonTerminal>();
    size_t oldLen = 0;
    do
    {
        oldLen = derivateEmptyTerminalSet->size();
        for (auto Vn : this->m_production)
        {
            // 若Vn左部非终结符已经在终结符号中,跳过
            if (derivateEmptyTerminalSet->find(Vn.first) != derivateEmptyTerminalSet->end())
            {
                continue;
            }
            for (auto express : Vn.second)
            {
                // 处理 A -> ε
                if (express == EMPTY)
                {
                    // 移除 A -> ε 的产生式
                    this->m_production[Vn.first].erase(express);
                    derivateEmptyTerminalSet->insert(Vn.first);
                    break;
                }

                // 处理 A -> BCDEFG...
                // 要使的 A -> ε, 则必须要有 B -> ε && C -> ε && D -> ε && ...
                // 注意:: string 迭代时 \0 不在迭代范围
                for (auto c : express)
                {
                    if (derivateEmptyTerminalSet->find(c) == derivateEmptyTerminalSet->end())
                    {
                        // A -> ε 不存在时,跳过前面插入语句
                        goto next;
                    }
                }

                derivateEmptyTerminalSet->insert(Vn.first);
                break;
            // A -> ε 不存在时,跳过前面插入语句
            next:
                continue;
            }
        }
    } while (oldLen != derivateEmptyTerminalSet->size());

    return derivateEmptyTerminalSet;
}

// 文法产生式---消除ε表达式
Grammar &Grammar::RemoveEmptyExpression()
{
    auto derivateEmptyTerminalSet = this->DerivateEmptyExpress();
    // 用于暂时存储修改的VnSet
    char tmp = '#';
    // 用于对文法进行替换的临时链表
    std::set<std::string> tmpExpresses;
    // 用于记录迭代前各个非终结符的产生式的个数
    std::map<NonTerminal, size_t> total;
    // 记录Vnset在表达式中的索引值
    size_t pos = 0;
    // 遍历存在产生式为 ε 的集合
    // 替换:
    // A -> ε
    // S -> Ab
    // S -> Ab | b
    for (auto VnSet : *derivateEmptyTerminalSet)
    {
        // 遍历所有的产生式
        for (auto ite_production = this->m_production.begin(); ite_production != this->m_production.end(); ite_production++)
        {
            // 在一个新的Vn开始前清空临时集合
            tmpExpresses.clear();
            // 对每一条产生式进行处理,去除时用 tmp 字符代替 VnSet字符,并保存进 expresses链表
            // 替代结束之后遍历链表并将 tmp 字符 还原为 VnSet 字符保存进 this->m_production 产生式中
            for (auto ite_express = ite_production->second.begin(); ite_express != ite_production->second.end(); ite_express++)
            {
                extractEmpty(0, VnSet, *ite_express, tmpExpresses);
            }
            for (auto expr : tmpExpresses)
            {
                ite_production->second.insert(expr);
            }
        }
    }

    delete derivateEmptyTerminalSet;
}

// 递归处理含有 Vn -> ε 的表达式
// 例子:
// A -> ε, S -> AcAdA 扫描过的A 用 # 代替
// AcAdA----
//          |----#cAdA
//          |           |---- #c#dA
//          |           |           |---- #c#d#
//          |           |           |---- #c#d
//          |           |---- #cdA
//          |                       |---- #cd#
//          |                       |---- #cd
//          |----cAdA
//                      |---- c#dA
//                      |           |---- c#d#
//                      |           |---- c#d
//                      |---- cdA
//                                  |---- cd#
//                                  |---- cd
//
static void extractEmpty(int startPos, NonTerminal Vn, std::string express, std::set<std::string> &expressSet)
{
    size_t pos = -1;
    // 递归出口,查不到记录
    if (-1 == (pos = express.find(Vn, startPos)))
    {
        return;
    }
    // 处理没有去除Vn的表达式
    // pos+1 表示从Vn的下一个开始,因为这个索引为pos的Vn已经做过处理,因而需要忽略该值
    extractEmpty(pos + 1, Vn, express, expressSet);
    express.erase(pos, 1);
    // 排除空串
    // 若产生式为 S -> Vn
    //  则 应该产生的只有 S -> Vn
    if (0 != express.size())
    {
        expressSet.insert(express);
        // 处理去掉Vn的表达式
        // pos 不加1, 因为Vn已经被去掉了
        extractEmpty(pos, Vn, express, expressSet);
    }
}

// 消除左递归,要求所有产生式中不存在能够推导出 ε 的式子
void Grammar::ExtractLeftRecursion()
{
    // 存储给定顺序的重排的非终结符号
    std::vector<NonTerminal> nonTerminalArray;
    // 记录非终结符个数
    size_t size = this->m_nonterminalSet.size();
    // 存放需要删除的产生式
    std::set<std::string> deletedProductions;
    // 存放需要插入的产生式
    std::set<std::string> insertedProductions;

    // 排列非终结符
    for (NonTerminal ch : this->m_nonterminalSet)
    {
        nonTerminalArray.push_back(ch);
    }

    // 将间接左递归转为直接左递归(若有)
    for (size_t i = 0; i < size; i++)
    {
        auto &productionSet = this->m_production[nonTerminalArray[i]];
        // 取一条产生式
        for (auto production = productionSet.begin(); production != productionSet.end(); production++)
        {
            // Aj
            for (size_t j = 0; j < i; j++)
            {
                // 替换
                auto subNonTerminalArray = std::vector<NonTerminal>(nonTerminalArray.begin(), nonTerminalArray.begin() + i);
                this->indirectRecursionToDirect(*production, nonTerminalArray[i], subNonTerminalArray, deletedProductions, insertedProductions);
            }
        }

        if (deletedProductions.size() != 0)
        {
            for (auto del : deletedProductions)
            {
                productionSet.erase(del);
            }
        }

        if (insertedProductions.size() != 0)
        {
            for (auto ins : insertedProductions)
            {
                productionSet.insert(ins);
            }
        }

        deletedProductions.clear();
        insertedProductions.clear();
    }

    // 消除直接左递归
    this->ExtractDirectRecursion();

    // 化简,消除不可达表达式
    this->ExtractUnReachableProductions();

    nonTerminalArray.clear();
}

// 转化间接左递归为直接左递归(若有)
void Grammar::indirectRecursionToDirect(std::string express, NonTerminal Vn, std::vector<NonTerminal> &VnSet, std::set<std::string> &deletedProductions, std::set<std::string> &insertedProductions)
{
    NonTerminal ch = express[0];

    if (!this->IsNonTerminal(ch) || VnSet.end() == std::find(VnSet.begin(), VnSet.end(), ch))
    {
        // Vn -> ch
        // ch 不在 Vn 中, 无需推导
        if (Vn != EMPTY_CHAR)
        {
            return;
        }
        insertedProductions.insert(express);
        return;
    }

    auto &productions = this->m_production[ch];
    for (auto pro : productions)
    {
        auto ss = (pro + express.substr(1)).c_str();
        this->indirectRecursionToDirect(pro + express.substr(1), EMPTY_CHAR, VnSet, deletedProductions, insertedProductions);
    }

    if (Vn != EMPTY_CHAR)
    {
        deletedProductions.insert(express);
        return;
    }
}

// 消除直接左递归
bool Grammar::ExtractDirectRecursion()
{
    NonTerminal allocNonTerminal = -1;
    NonTerminal Vn;
    bool hasLeftRecrusion = false;
    // 除去含有左递归之后的产生式的集合的副本
    std::set<Prodution> tmpProduction;

    for (auto ite = this->m_production.begin(); ite != this->m_production.end(); ite++)
    {
        Vn = ite->first;
        allocNonTerminal = -1;
        hasLeftRecrusion = false;
        tmpProduction.clear();

        // 判断是否存在左递归
        for (auto expression : ite->second)
        {
            if (expression[0] == Vn)
            {
                hasLeftRecrusion = true;
                break;
            }
        }

        if (!hasLeftRecrusion)
        {
            continue;
        }

        // 尝试分配新的非终结符
        if ((allocNonTerminal = this->AllocNonTerminal()) == -1)
        {
            return false;
        }

        // 消除递归
        // A -> Ab | a     ----------- 1
        // A -> abB        ----------- 2
        // B -> bB | ε     ----------- 3

        // 将左递归部分从productionSet中删除,并改成有递归的形式加入allocNonTermianl集合
        // 第3步实现
        for (auto expression = ite->second.begin(); expression != ite->second.end(); expression++)
        {
            if ((*expression)[0] == Vn)
            {
                auto tmp = *expression;
                expression = ite->second.erase(expression);
                this->m_production[allocNonTerminal].insert(tmp.erase(0, 1) + allocNonTerminal);
            }
        }

        // 第2步实现
        tmpProduction = ite->second;
        ite->second.clear();
        for (auto express : tmpProduction)
        {
            ite->second.insert(express + allocNonTerminal);
        }
        this->m_production[allocNonTerminal].insert(EMPTY);
    }
}

// 申请新的非终结符,目前最多申请26个(即ABCD...)
NonTerminal Grammar::AllocNonTerminal()
{
    NonTerminal termianl = this->m_NextAllocPos;
    if (!(termianl >= NON_TERMINAL_BEGIN && termianl <= NON_TERMINAL_END))
    {
        this->m_error = "超过所能分配的非终结符号的最大值\n";
        return -1;
    }

    while (true)
    {
        // 已登记的非终结符表中不存在时,则可以分配
        if (this->m_nonterminalSet.find(termianl) == this->m_nonterminalSet.end())
        {
            this->m_NextAllocPos++;
            this->m_nonterminalSet.insert(termianl);
            return termianl;
        }

        termianl = ++this->m_NextAllocPos;
        if (!(termianl >= NON_TERMINAL_BEGIN && termianl <= NON_TERMINAL_END))
        {
            this->m_error = "超过所能分配的非终结符号的最大值\n";
            return -1;
        }
    }
}

// 化简产生式,消除不可达表达式
// 从文法开始符开始,找出所有可以推导出来的非终结符
// 使用深度优先算法思路
void Grammar::ExtractUnReachableProductions()
{
    std::set<NonTerminal> reach;
    // 文法开始符
    NonTerminal start = this->m_S;
    // 文法开始符号开始的文法产生式
    std::set<Prodution> productions = this->m_production[start];
    size_t oldReachSize = 0;

    this->extractUnReachableProductions(start, reach);

    this->m_nonterminalSet.clear();
    this->m_nonterminalSet = reach;
    // 重置分配非终结符号指针
    this->m_NextAllocPos = NON_TERMINAL_BEGIN;
}

// 深度算法
void Grammar::extractUnReachableProductions(NonTerminal ch, std::set<NonTerminal> &reach)
{
    std::queue<NonTerminal> nonTerminalQueue;
    std::set<Prodution> *VnProduction;
    // ch 符号已经遍历过
    if (reach.find(ch) != reach.end() || !this->IsNonTerminal(ch))
    {
        return;
    }

    reach.insert(ch);
    VnProduction = &(this->m_production[ch]);

    // 将ch的产生式中所有的非终结符保存进队列,排除已存在reach的非终结符
    for (auto production : *VnProduction)
    {
        for (auto c : production)
        {
            // this->m_production.find(c) != this->m_production.end() 为了防止创建一个新的this->m_production[c]项
            // && this->m_production.find(c) != this->m_production.end() &&this->m_production[c].size() > 0 // 不可达情况,暂未考虑,假设全部可达
            // A->BC B->C C不可达,因此这一条理论上应该是不合法的,未考虑这个情况
            // A->BC B->C C->a 默认都是这种可达的产生式
            if (this->IsNonTerminal(c) && reach.find(c) == reach.end())
            {
                nonTerminalQueue.push(c);
            }
        }
    }

    while (nonTerminalQueue.size() != 0)
    {
        this->extractUnReachableProductions(nonTerminalQueue.front(), reach);
        nonTerminalQueue.pop();
    }
}

bool Grammar::IsNonTerminal(NonTerminal ch)
{
    return this->m_nonterminalSet.find(ch) != this->m_nonterminalSet.end();
}

bool Grammar::IsTerminal(Terminal ch)
{
    return this->m_termianlSet.find(ch) != this->m_termianlSet.end();
}


#pragma once
// NexiaCompiler v2.0 — AST Optimizer
// Ported from Optimizer.cs

#include <memory>
#include "ast.h"
#include <string>
#include <vector>
#include <cstdint>

namespace nexia {

enum class OptimizationLevel : uint8_t {
    O0 = 0, // No optimization
    O1 = 1, // Basic: constant folding, dead code
    O2 = 2, // Standard: + strength reduction, algebraic
    Os = 3  // Size: minimize code size
};

class AstOptimizer {
public:
    explicit AstOptimizer(OptimizationLevel level);

    void optimize(TranslationUnit& unit);

    int fold_count() const { return fold_count_; }
    int dead_code_count() const { return dead_code_count_; }
    int strength_reduction_count() const { return strength_count_; }
    int total_optimizations() const { return fold_count_ + dead_code_count_ + strength_count_; }

private:
    OptimizationLevel level_;
    int fold_count_ = 0;
    int dead_code_count_ = 0;
    int strength_count_ = 0;

    AstPtr optimize_node(AstPtr node);
    std::unique_ptr<FunctionDefinition> optimize_function(std::unique_ptr<FunctionDefinition> func);
    BlockPtr optimize_block(BlockPtr block);
    AstPtr optimize_statement(AstPtr stmt);

    AstPtr optimize_if(std::unique_ptr<IfStatement> ifStmt);
    AstPtr optimize_while(std::unique_ptr<WhileStatement> whileStmt);
    AstPtr optimize_for(std::unique_ptr<ForStatement> forStmt);

    ExprPtr optimize_expression(ExprPtr expr);
    ExprPtr optimize_binary(std::unique_ptr<BinaryExpr> bin);
    ExprPtr optimize_unary(std::unique_ptr<UnaryExpr> un);
    ExprPtr optimize_ternary(std::unique_ptr<TernaryExpr> tern);

    static bool try_parse_int(const std::string& text, int64_t& value);
    static bool try_evaluate_bool(const Expression& expr, bool& result);
    static bool is_int_literal(const Expression& expr, int64_t expected);
    static bool is_power_of_2(int64_t val);
    static int log2(int64_t val);

    ExprPtr try_strength_reduction(ExprPtr left, const std::string& op, ExprPtr right,
                                   int line, int col);
    ExprPtr try_algebraic_simplify(ExprPtr left, const std::string& op, ExprPtr right,
                                   int line, int col);

    void eliminate_dead_code(TranslationUnit& unit);
};

class PeepholeOptimizer {
public:
    void optimize(std::vector<uint32_t>& instructions);

    int removed_count() const { return removed_count_; }
    int replaced_count() const { return replaced_count_; }
    int total_optimizations() const { return removed_count_ + replaced_count_; }

private:
    int removed_count_ = 0;
    int replaced_count_ = 0;
};

} // namespace nexia

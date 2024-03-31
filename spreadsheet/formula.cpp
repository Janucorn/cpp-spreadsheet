#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

FormulaError::FormulaError(Category category)
    : category_(category) {}

FormulaError::Category FormulaError::GetCategory() const {
    return category_;
}

bool FormulaError::operator==(FormulaError rhs) const {
    return category_ == rhs.category_;
}

std::string_view FormulaError::ToString() const {
    switch (category_) {
    case Category::Ref:
        return "#REF!";
    case Category::Value:
        return "#VALUE!";
    case Category::Arithmetic:
        return "#ARITHM!";
    }
    return "";
}

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << fe.ToString();
}

namespace {
    class Formula : public FormulaInterface {
    public:
        // Реализуйте следующие методы:
        explicit Formula(std::string expression) 
            try : ast_(ParseFormulaAST(expression)) {
        } catch (const std::exception& exc) {
            throw FormulaException("Invalid formula"s);
        }

        Value Evaluate(const SheetInterface& sheet) const override {
            const std::function<double(Position)> args = [&sheet](const Position p)->double {
                if (!p.IsValid()) {
                    throw FormulaError(FormulaError::Category::Ref);
                }

                const auto* cell = sheet.GetCell(p);
                if (cell == nullptr) {
                    return 0.0;
                }
                const auto& value = cell->GetValue();
                if (std::holds_alternative<double>(value)) {
                    return std::get<double>(value);
                }
                if (std::holds_alternative<std::string>(value)) {
                    double result = 0;
                    if (!std::get<std::string>(value).empty()) {
                        std::istringstream in(std::get<std::string>(value));
                        if (!(in >> result) || !in.eof()) {
                            throw FormulaError(FormulaError::Category::Value);
                        }
                    }
                    return result;
                }
                throw FormulaError(std::get<FormulaError>(value));
                };

            try {
                return ast_.Execute(sheet);
            }
            catch (const FormulaError& fe) {
                return fe;
            }
        }

        std::string GetExpression() const override {
            std::ostringstream out;
            ast_.PrintFormula(out);
            return out.str();
        }

        std::vector<Position> GetReferencedCells() const override {
            std::set<Position> cells;
            for (auto cell : ast_.GetCells()) {
                if (cell.IsValid()) {
                    cells.insert(cell);
                }
            }
            return { cells.begin(), cells.end() };
        }

    private:
        FormulaAST ast_;
        std::vector<Position> referenced_cells_;
    };


}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}
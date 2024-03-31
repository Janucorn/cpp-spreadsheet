#include "cell.h"
#include "sheet.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <stack>

// class Cell::Impl
class Cell::Impl {
public:
	virtual ~Impl() = default;
	virtual Value GetValue() const = 0;
	virtual std::string GetText() const = 0;
	virtual bool IsCacheValid() const {
		return true;
	}

	virtual std::vector<Position> GetReferencedCells() const = 0;
	
	virtual void InvalidateCache() {};

protected:
	std::string text_;
	Value value_;
};

class Cell::EmptyImpl : public Impl {
public:
	EmptyImpl() {
		value_ = text_ = "";
	}
	Value GetValue() const override {
		return value_;
	}
	std::string GetText() const override {
		return text_;
	}
	std::vector<Position> GetReferencedCells() const override {
		return {};
	}
};

class Cell::TextImpl : public Impl {
public:
	TextImpl(std::string_view text) {
		text_ = text;
		if (text[0] == ESCAPE_SIGN) {
			text = text.substr(1);
		}
		value_ = std::string(text);
	}

	Value GetValue() const override {
		return value_;
	}

	std::string GetText() const override {
		return text_;
	}

	std::vector<Position> GetReferencedCells() const override {
		return {};
	}
};

class Cell::FormulaImpl : public Impl {
public:
	// то, что следует после знака "=", является выражением формулы.
	// именно оно - аргумент метода ParseFormula из файла formula.h
	explicit FormulaImpl(std::string_view expression, const SheetInterface& sheet) 
		: sheet_(sheet) {
		expression = expression.substr(1);
		//value_ = std::string(expression);
		formula_ = ParseFormula(std::string(expression));
		text_ = FORMULA_SIGN + formula_->GetExpression();
	}

	Value GetValue() const override {
		// Функця Evaluate() возвращает Formula::Value = std::variant<double, FormulaError>
		if (!cache_) {
			cache_ = formula_->Evaluate(sheet_);
		}

		auto value = formula_->Evaluate(sheet_);
		if (std::holds_alternative<double>(value)) {
			return std::get<double>(value);
		}
		return std::get<FormulaError>(value);
	}

	std::string GetText() const override {
		return text_;
	}

	bool IsCacheValid() const override {
		return cache_.has_value();
	}

	void InvalidateCache() override {
		cache_.reset();
	}

	std::vector<Position> GetReferencedCells() const override {
		return formula_->GetReferencedCells();
	}

	
private:
	// mutable позволяет изменять поле у константных объектов
	mutable std::optional<FormulaInterface::Value> cache_;
	std::unique_ptr<FormulaInterface> formula_;
	const SheetInterface& sheet_;
};


Cell::Cell(Sheet& sheet)
	: impl_(std::make_unique<EmptyImpl>())	
	, sheet_(sheet) {}

Cell::~Cell() {}

void Cell::Set(std::string text) {
	std::unique_ptr<Impl> impl;
	if (text.empty()) {
		impl = std::make_unique<EmptyImpl>();
	} else if (text[0] == FORMULA_SIGN && text.size() > 1) {
		impl = std::make_unique<FormulaImpl>(std::move(text), sheet_);
	} else {
		impl = std::make_unique<TextImpl>(std::move(text));
	}
	
	if (IsCyclicDependency(*impl)) {
		throw CircularDependencyException("Circular dependency");
	}
	impl_ = std::move(impl);

	for (Cell* from : from_cells_) {
		from->from_cells_.erase(this);
	}

	// проходим по всем ячейкам, на которые ссылается данная ячейка
	for (const auto& pos : impl_->GetReferencedCells()) {
		Cell* cell = sheet_.GetCellPtr(pos);
		// при отсутсвии ячейки в таблице создаем новую пустую ячейку в таблице
		if (cell == nullptr) {
			sheet_.SetCell(pos, "");
			cell = sheet_.GetCellPtr(pos);
		}
		// создаем список ячеек, которые ссылаются на данную ячейку
		to_cells_.insert(cell);
		// указываем текущуюя ячейку, как ячейка, которая имеет ссылку на ссылочную ячейку
		cell->from_cells_.insert(this);
	}

	if (impl_->IsCacheValid()) {
		impl_->InvalidateCache();
		// очищаем кэш для тех ячеек, которые ссылаются на текующую ячейку
		for (Cell* from : from_cells_) {
			from->impl_->InvalidateCache();
		}
	}
}

void Cell::Clear() {
	impl_ = std::make_unique<EmptyImpl>();
}

Cell::Value Cell::GetValue() const {
	return impl_->GetValue();
}

std::string Cell::GetText() const {
	return impl_->GetText();
}

bool Cell::IsCyclicDependency(const Impl& new_impl) const {
	if (new_impl.GetReferencedCells().empty()) {
		return false;
	}

	std::unordered_set<const Cell*> referenced;
	std::unordered_set<const Cell*> visited;
	std::stack<const Cell*> to_visit;
	// список ячеек из формулы
	for (const auto& pos : new_impl.GetReferencedCells()) {
		referenced.insert(sheet_.GetCellPtr(pos));
	}
	to_visit.push(this);
	// проход по всем ссылочным ячейкам вглубь
	while (!to_visit.empty()) {
		const Cell* current = to_visit.top();
		to_visit.pop();
		visited.insert(current);

		// если в формульной ячейке есть ячейки со ссылкой из ячейки нижнего уровня
		if (referenced.find(current) != referenced.end()) {
			return true;
		}
		// добавляем в проверку ячейки, которые ссылаются на текущую, за исключением уже посещенных
		for (const Cell* cell : current->from_cells_) {
			if (visited.find(cell) == visited.end()) {
				to_visit.push(cell);
			}
		}
	}
	return false;
}

std::vector<Position> Cell::GetReferencedCells() const {
	return impl_->GetReferencedCells();
}

bool Cell::IsReferenced() const {
	return !from_cells_.empty();
}
#pragma once

#include "common.h"
#include "formula.h"

#include <functional>
#include <unordered_set>
#include <optional>

class Sheet;

class Cell : public CellInterface {
public:
	Cell(Sheet& sheet);
	~Cell();

	void Set(std::string text);
	void Clear();

	Value GetValue() const override;
	std::string GetText() const override;
	std::vector<Position> GetReferencedCells() const override;

	bool IsReferenced() const;

private:
	class Impl;
	class EmptyImpl;
	class TextImpl;
	class FormulaImpl;

	// Проверка на возникновение циклической зависимости 
	bool IsCyclicDependency(const Impl& new_impl) const;

	std::unique_ptr<Impl> impl_;
	Sheet& sheet_;
	// список ячеек, на которые ссылается данная ячейка
	std::unordered_set<Cell*> to_cells_;
	// список ячеек, которые ссылаются на данную ячейку
	std::unordered_set<Cell*> from_cells_;
};
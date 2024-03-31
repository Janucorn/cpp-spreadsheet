#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <optional>

using namespace std::literals;

Sheet::~Sheet() {}

void Sheet::SetCell(Position pos, std::string text) {
	if (!pos.IsValid()) {
		throw InvalidPositionException("Invalid position"s);
	}

	const auto& cell = mTable_.find(pos);
	if (cell == mTable_.end()) {
		mTable_.emplace(pos, std::make_unique<Cell>(*this));
	}
	mTable_.at(pos)->Set(std::move(text));
}

const CellInterface* Sheet::GetCell(Position pos) const {
	return GetCellPtr(pos);
}

CellInterface* Sheet::GetCell(Position pos) {
	return GetCellPtr(pos);
}

void Sheet::ClearCell(Position pos) {
	if (!pos.IsValid()) {
		throw InvalidPositionException("Invalid position"s);
	}
	// если pos вне таблицы
	if (GetCell(pos) == nullptr) {
		return;
	}
	const auto& cell = mTable_.find(pos);
	if (cell != mTable_.end() && cell->second != nullptr) {

		cell->second->Clear();
		if (!cell->second->IsReferenced()) {
			cell->second.reset();
		}
	}
}

Size Sheet::GetPrintableSize() const {
	Size result { 0, 0 };
	// проходим по таблице 
	if (!mTable_.empty()) {
		for (const auto& [pos, cell] : mTable_) {
			if (cell == nullptr || cell->GetText().empty()) {
				continue;
			}
			// размер увеличиваем на 1, т.к. индексация ячейеек начинается с (0, 0) 
			if (result.rows <= pos.row) {
				result.rows = pos.row + 1;
			}
			if (result.cols <= pos.col) {
				result.cols = pos.col + 1;
			}
		}
	}
	return result;
}

void Sheet::PrintValues(std::ostream& output) const {
	Size table_size = GetPrintableSize();
	for (int row = 0; row < table_size.rows; ++row) {
		bool is_first = true;
		for (int col = 0; col < table_size.cols; ++col) {
			const Position pos{ row, col };
			if (!is_first) {
				output << '\t';
			}
			is_first = false;
			if (GetCell(pos) != nullptr) {
				std::visit([&output](auto&& arg) {
					output << arg;
					}, GetCell(pos)->GetValue());
			}
		}
		output << '\n';
	}
	
}

void Sheet::PrintTexts(std::ostream& output) const {
	Size table_size = GetPrintableSize();
	for (int row = 0; row < table_size.rows; ++row) {
		bool is_first = true;
		for (int col = 0; col < table_size.cols; ++col) {
			const Position pos{ row, col };
			if (!is_first) {
				output << '\t';
			}
			is_first = false;
			if (GetCell(pos) != nullptr) {
				output << GetCell({ row,col })->GetText();
			}
		}
		output << '\n';
	}
}

const Cell* Sheet::GetCellPtr(Position pos) const {
	if (!pos.IsValid()) {
		throw InvalidPositionException("Invalid position"s);
	}
	if (mTable_.count(pos) == 0) {
		return nullptr;
	}
	const auto& cell = mTable_.at(pos);
	if (cell == nullptr) {
		return nullptr;
	}

	return cell.get();
}

Cell* Sheet::GetCellPtr(Position pos) {
	return const_cast<Cell*>(static_cast<const Sheet&>(*this).GetCellPtr(pos));
}

std::unique_ptr<SheetInterface> CreateSheet() {
	return std::make_unique<Sheet>();
}
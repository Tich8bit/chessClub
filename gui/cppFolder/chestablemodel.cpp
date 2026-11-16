#include "chestablemodel.h"

#include <QBrush>
#include <QColor>

ChessTableModel::ChessTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void ChessTableModel::resetRows(const QStringList &headers, const QVector<Row> &rows)
{
    beginResetModel();
    _headers = headers;
    _rows = rows;
    endResetModel();
}

int ChessTableModel::idAt(int row) const
{
    if (row < 0 || row >= _rows.size()) {
        return -1;
    }
    return _rows[row].id;
}

int ChessTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : _rows.size();
}

int ChessTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : _headers.size();
}

QVariant ChessTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= _rows.size()) {
        return {};
    }

    const Row &row = _rows[index.row()];

    if (role == Qt::DisplayRole) {
        return index.column() < row.text.size() ? row.text[index.column()] : QString();
    }
    if (role == SortRole) {
        return index.column() < row.values.size() ? row.values[index.column()] : QVariant();
    }
    if (role == Qt::ToolTipRole) {
        // Полный текст в подсказке: длинные ФИО обрезаются в колонке.
        return index.column() < row.text.size() ? row.text[index.column()] : QString();
    }
    if (role == Qt::ForegroundRole && row.muted) {
        return QBrush(QColor(140, 140, 140));
    }
    if (role == Qt::TextAlignmentRole) {
        // Числа по центру, текстовые колонки — по левому краю.
        const QVariant value = index.column() < row.values.size()
                                   ? row.values[index.column()]
                                   : QVariant();
        const int typeId = value.typeId();

        if (typeId == QMetaType::Int || typeId == QMetaType::LongLong
            || typeId == QMetaType::Double) {
            return QVariant(Qt::AlignCenter);
        }
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    }

    return {};
}

QVariant ChessTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return {};
    }
    if (orientation == Qt::Horizontal) {
        return section < _headers.size() ? _headers[section] : QString();
    }
    return section + 1;
}

#pragma once

#include <QAbstractTableModel>
#include <QStringList>
#include <QVariantList>
#include <QVector>

// Универсальная табличная модель: строки задаются готовыми текстами
// и значениями для сортировки. Идентификатор строки — ID объекта в клубе.
class ChessTableModel : public QAbstractTableModel
{
public:
    enum { SortRole = Qt::UserRole + 1 };

    struct Row {
        QStringList text;
        QVariantList values;
        int id = -1;
    };

    explicit ChessTableModel(QObject *parent = nullptr);

    void resetRows(const QStringList &headers, const QVector<Row> &rows);
    int idAt(int row) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    QStringList _headers;
    QVector<Row> _rows;
};

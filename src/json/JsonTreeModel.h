#ifndef JSONTREEMODEL_H
#define JSONTREEMODEL_H

#include "JsonSession.h"
#include <QAbstractItemModel>

namespace JsonTools {

class TreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit TreeModel(QObject *parent = nullptr) : QAbstractItemModel(parent) {}
    void setSnapshot(Snapshot snapshot);
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex & = {}) const override { return 4; }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex forNode(int node, int column = 0) const;
    int node(const QModelIndex &index) const { return index.isValid() ? int(index.internalId()) - 1 : -1; }

private:
    Snapshot document;
};

}

#endif
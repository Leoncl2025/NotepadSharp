#include "JsonTreeModel.h"

namespace JsonTools {

void TreeModel::setSnapshot(Snapshot snapshot)
{
    beginResetModel();
    document = std::move(snapshot);
    endResetModel();
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parentIndex) const
{
    if (!document || row < 0 || column < 0 || column >= 4 || (parentIndex.isValid() && parentIndex.column() != 0)) {
        return {};
    }
    const auto &children = parentIndex.isValid() ? document->nodes[node(parentIndex)].children : document->roots;
    return row < children.size() ? createIndex(row, column, quintptr(children[row] + 1)) : QModelIndex();
}

QModelIndex TreeModel::parent(const QModelIndex &child) const
{
    return document && child.isValid() ? forNode(document->nodes[node(child)].parent) : QModelIndex();
}

int TreeModel::rowCount(const QModelIndex &parentIndex) const
{
    if (!document || (parentIndex.isValid() && parentIndex.column() != 0)) {
        return 0;
    }
    return parentIndex.isValid() ? document->nodes[node(parentIndex)].children.size() : document->roots.size();
}

QModelIndex TreeModel::forNode(int id, int column) const
{
    if (!document || id < 0 || id >= document->nodes.size()) {
        return {};
    }
    const int parentId = document->nodes[id].parent;
    const auto &siblings = parentId < 0 ? document->roots : document->nodes[parentId].children;
    return createIndex(siblings.indexOf(id), column, quintptr(id + 1));
}

QVariant TreeModel::data(const QModelIndex &modelIndex, int role) const
{
    if (!document || !modelIndex.isValid()) {
        return {};
    }
    const int id = node(modelIndex);
    const Node &item = document->nodes[id];
    if (role == Qt::ToolTipRole) {
        return document->path(id) + '\n' + QString::fromUtf8(document->raw(id).left(4096));
    }
    if (role != Qt::DisplayRole) {
        return {};
    }
    switch (modelIndex.column()) {
    case 0:
        if (item.parent < 0) {
            return document->lines ? tr("Record %1").arg(item.record) : QStringLiteral("$");
        }
        return item.arrayIndex >= 0 ? QStringLiteral("[%1]").arg(item.arrayIndex) : item.key;
    case 1:
        if (item.kind == Kind::Object || item.kind == Kind::Array) {
            return tr("%1 items").arg(item.children.size());
        }
        return document->decoded(id).left(256).replace('\n', QStringLiteral("\\n")).replace('\r', QStringLiteral("\\r"));
    case 2: return item.complete ? Index::kindName(item.kind) : Index::kindName(Kind::Unknown);
    case 3: return document->lineAt(item.start);
    default: return {};
    }
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    const QStringList labels{tr("Key"), tr("Value"), tr("Type"), tr("Line")};
    return section >= 0 && section < labels.size() ? labels[section] : QVariant();
}

}
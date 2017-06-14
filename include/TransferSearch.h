#pragma once
#include <vector>


#include "WAAPITransfer.h"


class TransferSearch
{
    explicit TransferSearch(WAAPITransfer &transferObject)
        : m_transferObject(transferObject)
    {}

    void UpdateReaperState();
    void UpdateRenderItemIndexing();

private:
    WAAPITransfer &m_transferObject;
};
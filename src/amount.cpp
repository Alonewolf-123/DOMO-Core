// Copyright (c) 2009-2010 Domo Domo
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"

#include "tinyformat.h"

CFeeRate::CFeeRate(const CAmount& nFeePaid, size_t nSize)
{
    if (nSize > 0)
        nDomosPerK = nFeePaid*1000/nSize;
    else
        nDomosPerK = 0;
}

CAmount CFeeRate::GetFee(size_t nSize) const
{
    CAmount nFee = nDomosPerK*nSize / 1000;

    if (nFee == 0 && nDomosPerK > 0)
        nFee = nDomosPerK;

    return nFee;
}

std::string CFeeRate::ToString() const
{
    return strprintf("%d.%05d DOMO/kB", nDomosPerK / COIN, nDomosPerK % COIN);
}

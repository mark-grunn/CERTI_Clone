// ----------------------------------------------------------------------------
// CERTI - HLA RunTime Infrastructure
// Copyright (C) 2004  ONERA
//
// This file is part of CERTI
//
// CERTI is free software ; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation ; either version 2 of the License, or
// (at your option) any later version.
//
// CERTI is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY ; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program ; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
// $Id: BillardDDM.hh,v 3.8 2005/02/09 16:17:28 breholee Exp $
// ----------------------------------------------------------------------------

#ifndef CERTI_BILLARD_DDM_HH
#define CERTI_BILLARD_DDM_HH

#include "RTI.hh"
#include "BillardNull.hh"
#include "Billard.hh"

struct Area {
    Region *region ;
    long x ;
    long y ;
    long size ;
};

class BillardDDM : public Billard
{
public:
    BillardDDM(std::string);
    virtual ~BillardDDM();

    virtual void checkRegions();
    virtual void declare();
    virtual void publishAndSubscribe();

protected:
    std::vector<Area> areas ;
    int subRegion ;
    int pubRegion ;
    SpaceHandle GeoID ;
};

#endif // CERTI_BILLARD_DDM_HH

// $Id: BillardDDM.hh,v 3.8 2005/02/09 16:17:28 breholee Exp $
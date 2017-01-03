/* This file is part of COVISE.

You can use it under the terms of the GNU Lesser General Public License
version 2.1 or later, see lgpl - 2.1.txt.

* License: LGPL 2 + */


#ifndef OSCPARAMETERLIST_H
#define OSCPARAMETERLIST_H

#include "../oscExport.h"
#include "../oscObjectBase.h"
#include "../oscObjectVariable.h"
#include "../oscObjectVariableArray.h"

#include "../oscVariables.h"
#include "oscParameter.h"

namespace OpenScenario
{
class OPENSCENARIOEXPORT oscParameterList : public oscObjectBase
{
public:
oscParameterList()
{
        OSC_OBJECT_ADD_MEMBER(Parameter, "oscParameter");
    };
    oscParameterArrayMember Parameter;

};

typedef oscObjectVariable<oscParameterList *> oscParameterListMember;
typedef oscObjectVariableArray<oscParameterList *> oscParameterListArrayMember;


}

#endif //OSCPARAMETERLIST_H
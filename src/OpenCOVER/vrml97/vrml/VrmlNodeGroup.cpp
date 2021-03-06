/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

//
//  Vrml 97 library
//  Copyright (C) 1998 Chris Morley
//
//  %W% %G%
//
//  VrmlNodeGroup.cpp
//

#include "VrmlNodeGroup.h"

#include "VrmlNodeType.h"

#include "VrmlNodeProto.h"
#include "VrmlNodePlaneSensor.h"
#include "VrmlNodeSpaceSensor.h"
#include "VrmlNodeTouchSensor.h"
#include "VrmlNodeSphereSensor.h"
#include "VrmlNodeCylinderSensor.h"

#include "VrmlMFNode.h"
#include "VrmlSFVec3f.h"
#include "VrmlNodeGeometry.h"
#include "VrmlNodeProto.h"
#include "VrmlNodeShape.h"

using std::cerr;
using std::endl;
using namespace vrml;

// Return a new VrmlNodeGroup
static VrmlNode *creator(VrmlScene *s) { return new VrmlNodeGroup(s); }

// Define the built in VrmlNodeType:: "Group" fields

VrmlNodeType *VrmlNodeGroup::defineType(VrmlNodeType *t)
{
    static VrmlNodeType *st = 0;

    if (!t)
    {
        if (st)
            return st;
        t = st = new VrmlNodeType("Group", creator);
    }

    VrmlNodeChild::defineType(t); // Parent class
    t->addEventIn("addChildren", VrmlField::MFNODE);
    t->addEventIn("removeChildren", VrmlField::MFNODE);
    t->addExposedField("children", VrmlField::MFNODE);
    t->addField("bboxCenter", VrmlField::SFVEC3F);
    t->addField("bboxSize", VrmlField::SFVEC3F);

    return t;
}

VrmlNodeType *VrmlNodeGroup::nodeType() const { return defineType(0); }

VrmlNodeGroup::VrmlNodeGroup(VrmlScene *scene)
    : VrmlNodeChild(scene)
    , d_bboxSize(-1.0, -1.0, -1.0)
    , d_parentTransform(0)
    , d_viewerObject(0)
{
}

VrmlNodeGroup::~VrmlNodeGroup()
{
    // delete d_viewerObject...
    while (d_children.size())
    {
        // don't, this causes an endless loop if(d_children[0])
        //{
        d_children.removeNode(d_children[0]);
        //}
    }
}

void VrmlNodeGroup::flushRemoveList()
{
    while (d_childrenToRemove.size())
    {
        d_childrenToRemove.removeNode(d_childrenToRemove[0]);
    }
}

//

VrmlNode *VrmlNodeGroup::cloneMe() const
{
    return new VrmlNodeGroup(*this);
}

void VrmlNodeGroup::cloneChildren(VrmlNamespace *ns)
{
    int n = d_children.size();
    VrmlNode **kids = d_children.get();
    for (int i = 0; i < n; ++i)
    {
        if (!kids[i])
            continue;
        VrmlNode *newKid = kids[i]->clone(ns)->reference();
        kids[i]->dereference();
        kids[i] = newKid;
        kids[i]->parentList.push_back(this);
    }
}

VrmlNodeGroup *VrmlNodeGroup::toGroup() const
{
    return (VrmlNodeGroup *)this;
}

bool VrmlNodeGroup::isModified() const
{
    if (d_modified)
        return true;

    int n = d_children.size();

    for (int i = 0; i < n; ++i)
    {
        if (d_children[i] == NULL)
            return false;
        if (d_children[i]->isModified())
            return true;
    }

    return false;
}

void VrmlNodeGroup::clearFlags()
{
    VrmlNode::clearFlags();

    int n = d_children.size();
    for (int i = 0; i < n; ++i)
        if (d_children[i])
            d_children[i]->clearFlags();
}

void VrmlNodeGroup::addToScene(VrmlScene *s, const char *relativeUrl)
{
    d_scene = s;

    nodeStack.push_front(this);
    System::the->debug("VrmlNodeGroup::addToScene( %s )\n",
                       relativeUrl ? relativeUrl : "<null>");

    const char *currentRel = d_relative.get();
    if (!currentRel || !relativeUrl || strcmp(currentRel, relativeUrl) != 0)
        d_relative.set(relativeUrl);

    int n = d_children.size();

    for (int i = 0; i < n; ++i)
    {
        if (d_children[i])
            d_children[i]->addToScene(s, d_relative.get());
    }
    nodeStack.pop_front();
}

// Copy the routes to nodes in the given namespace.

void VrmlNodeGroup::copyRoutes(VrmlNamespace *ns)
{
    nodeStack.push_front(this);
    VrmlNode::copyRoutes(ns); // Copy my routes

    // Copy childrens' routes
    int n = d_children.size();
    for (int i = 0; i < n; ++i)
        if (d_children[i])
            d_children[i]->copyRoutes(ns);
    nodeStack.pop_front();
}

VrmlNode *VrmlNodeGroup::getParentTransform() { return d_parentTransform; }

bool VrmlNodeGroup::isOnlyGeometry() const
{
    if (!VrmlNode::isOnlyGeometry())
        return false;

    int n = d_children.size();
    for (int i=0; i<n; ++i)
    {
        if (!d_children[i]->isOnlyGeometry())
        {
            //std::cerr << "Nc" << i << std::flush;
            return false;
        }
    }

    return true;
}

std::ostream &VrmlNodeGroup::printFields(std::ostream &os, int indent)
{
    if (d_bboxCenter.x() != 0.0 || d_bboxCenter.z() != 0.0 || d_bboxCenter.y() != 0.0)
        PRINT_FIELD(bboxCenter);
    if (d_bboxSize.x() != -1.0 || d_bboxSize.z() != -1.0 || d_bboxSize.y() != -1.0)
        PRINT_FIELD(bboxSize);
    if (d_children.size() > 0)
        PRINT_FIELD(children);

    return os;
}

void VrmlNodeGroup::checkAndRemoveNodes(Viewer *viewer)
{
    if (d_childrenToRemove.size())
    {
        viewer->beginObject(name(), 0, this);
        int i, n = d_childrenToRemove.size();
        for (i = 0; i < n; i++)
        {
            Viewer::Object child_viewerObject = 0;
            VrmlNode *kid = d_childrenToRemove[i];
            if (kid->toGeometry())
                child_viewerObject = kid->toGeometry()->getViewerObject();
            else if (kid->toGroup())
                child_viewerObject = kid->toGroup()->d_viewerObject;
            else if (kid->toProto())
                child_viewerObject = kid->toProto()->getViewerObject();
            else if (kid->toShape())
                child_viewerObject = kid->toShape()->getViewerObject();
            if (child_viewerObject)
                viewer->removeChild(child_viewerObject);
        }
        viewer->endObject();
    }
    while (d_childrenToRemove.size())
    {
        d_childrenToRemove.removeNode(d_childrenToRemove[0]);
    }
}
// Render each of the children

void VrmlNodeGroup::render(Viewer *viewer)
{
    if (!haveToRender())
    {
        return;
    }

    if (d_viewerObject && isModified())
    {
        viewer->removeObject(d_viewerObject);
        d_viewerObject = 0;
    }
    checkAndRemoveNodes(viewer);

    if (d_viewerObject)
        viewer->insertReference(d_viewerObject);

    else if (d_children.size() > 0)
    {
        int i, n = d_children.size();
        int nSensors = 0;

        d_viewerObject = viewer->beginObject(name(), 0, this);

        // Draw nodes that impact their siblings (DirectionalLights,
        // TouchSensors, any others? ...)
        for (i = 0; i < n; ++i)
        {
            VrmlNode *kid = d_children[i];

            //if ( kid->toLight() ) && ! (kid->toPointLight() || kid->toSpotLight()) )
            //  kid->render(viewer);
            //else
            if (kid && ((kid->toTouchSensor() && kid->toTouchSensor()->isEnabled()) || (kid->toPlaneSensor() && kid->toPlaneSensor()->isEnabled()) || (kid->toCylinderSensor() && kid->toCylinderSensor()->isEnabled()) || (kid->toSphereSensor() && kid->toSphereSensor()->isEnabled()) || (kid->toSpaceSensor() && kid->toSpaceSensor()->isEnabled())))
            {
                if (++nSensors == 1)
                    viewer->setSensitive(this);
            }
        }

        // Do the rest of the children (except the scene-level lights)
        for (i = 0; i < n; ++i)
        {
            if (d_children[i])
            {
                if (!(/*d_children[i]->toLight() ||*/
                      d_children[i]->toPlaneSensor() || d_children[i]->toCylinderSensor() || d_children[i]->toSpaceSensor() || d_children[i]->toTouchSensor()))
                    d_children[i]->render(viewer);
            }
        }

        // Turn off sensitivity
        if (nSensors > 0)
            viewer->setSensitive(0);

        viewer->endObject();
    }

    clearModified();
}

// Accumulate transforms
// Cache a pointer to (one of the) parent transforms for proper
// rendering of bindables.

void VrmlNodeGroup::accumulateTransform(VrmlNode *parent)
{
    d_parentTransform = parent;

    int i, n = d_children.size();

    for (i = 0; i < n; ++i)
    {
        VrmlNode *kid = d_children[i];
        if (kid)
            kid->accumulateTransform(parent);
    }
}

// Pass on to enabled touchsensor child.

void VrmlNodeGroup::activate(double time,
                             bool isOver, bool isActive,
                             double *p, double *M)
{
    int i, n = d_children.size();

    for (i = 0; i < n; ++i)
    {
        VrmlNode *kid = d_children[i];

        if (kid == NULL)
            continue;
        if (kid->toTouchSensor() && kid->toTouchSensor()->isEnabled())
        {
            kid->toTouchSensor()->activate(time, isOver, isActive, p);
            break;
        }
        else if (kid->toPlaneSensor() && kid->toPlaneSensor()->isEnabled())
        {
            kid->toPlaneSensor()->activate(time, isActive, p);
            break;
        }
        else if (kid->toSpaceSensor() && kid->toSpaceSensor()->isEnabled())
        {
            kid->toSpaceSensor()->activate(time, isActive, p, M);
            break;
        }
        else if (kid->toSphereSensor() && kid->toSphereSensor()->isEnabled())
        {
            kid->toSphereSensor()->activate(time, isActive, p);
            break;
        }
        else if (kid->toCylinderSensor() && kid->toCylinderSensor()->isEnabled())
        {
            kid->toCylinderSensor()->activate(time, isActive, p);
            break;
        }
    }
}

void VrmlNodeGroup::addChildren(const VrmlMFNode &children)
{
    int nNow = d_children.size();
    int n = children.size();

    for (int i = 0; i < n; ++i)
    {
        VrmlNode *child = children[i];
        if (child == NULL)
        {
            continue;
        }

        child->parentList.push_back(this);
        if (child->getTraversalForce() > 0)
        {
            forceTraversal(false, child->getTraversalForce());
        }

        VrmlNodeProto *p = 0;

        // Add legal children and un-instantiated EXTERNPROTOs
        // Is it legal to add null children nodes?
        if (child == 0 || child->toChild() || ((p = child->toProto()) != 0 && p->size() == 0))
        {
            d_children.addNode(child);
            if (child)
            {
                child->addToScene(d_scene, d_relative.get());
                child->accumulateTransform(d_parentTransform);
            }
        }
        else
            System::the->error("Error: Attempt to add a %s node as a child of a %s node.\n",
                               child->nodeType()->getName(), nodeType()->getName());
    }

    if (nNow != d_children.size())
    {
        //??eventOut( d_scene->timeNow(), "children_changed", d_children );
        setModified();
    }
}

void VrmlNodeGroup::removeChildren(const VrmlMFNode &children)
{
    int nNow = d_children.size();
    int n = children.size();

    for (int i = 0; i < n; ++i)
    {
        if (children[i])
        {
            children[i]->decreaseTraversalForce();
            children[i]->parentList.remove(this);
        }
        else
        {
            cerr << "VrmlNodeGroup::removeChildren1: NULL child" << endl;
        }
        d_childrenToRemove.addNode(children[i]);
        d_children.removeNode(children[i]);
    }

    if (nNow != d_children.size())
    {
        //??eventOut( d_scene->timeNow(), "children_changed", d_children );
        setModified();
    }
}

void VrmlNodeGroup::removeChildren()
{
    int n = d_children.size();

    for (int i = n; i > 0; --i)
    {
        if (d_children[i - 1])
        {
            d_children[i - 1]->decreaseTraversalForce();
            d_children[i - 1]->parentList.remove(this);
        }
        else
        {
            cerr << "VrmlNodeGroup::removeChildren2: NULL child" << endl;
        }
        d_childrenToRemove.addNode(d_children[i - 1]);
        d_children.removeNode(d_children[i - 1]);
    }

    setModified();
}

void VrmlNodeGroup::eventIn(double timeStamp,
                            const char *eventName,
                            const VrmlField *fieldValue)
{
    if (!fieldValue)
        return;

    if (strcmp(eventName, "addChildren") == 0)
    {
        if (fieldValue->toMFNode()) // check that fieldValue is MFNode
            addChildren(*(fieldValue->toMFNode()));
        else
            System::the->error("VrmlNodeGroup.%s %s eventIn invalid field type.\n",
                               name(), eventName);
    }

    else if (strcmp(eventName, "removeChildren") == 0)
    {
        if (fieldValue->toMFNode()) // check that fieldValue is MFNode
            removeChildren(*(fieldValue->toMFNode()));
        else
            System::the->error("VrmlNodeGroup.%s %s eventIn invalid field type.\n",
                               name(), eventName);
    }

    else if ((strcmp(eventName, "children") == 0) || (strcmp(eventName, "set_children") == 0))
    {
        removeChildren();
        if (fieldValue->toMFNode()) // check that fieldValue is MFNode
            addChildren(*(fieldValue->toMFNode()));
        else
            System::the->error("VrmlNodeGroup.%s %s eventIn invalid field type.\n",
                               name(), eventName);
    }

    else
    {
        VrmlNode::eventIn(timeStamp, eventName, fieldValue);
    }
}

// Set the value of one of the node fields.
void VrmlNodeGroup::setField(const char *fieldName,
                             const VrmlField &fieldValue)
{
    if
        TRY_FIELD(bboxCenter, SFVec3f)
    else if
        TRY_FIELD(bboxSize, SFVec3f)
    else if (!strcmp(fieldName, "children"))
    {
        if (fieldValue.toMFNode())
        {
            for (int i = 0; i < d_children.size(); i++)
            {
                if (d_children[i])
                {
                    d_children[i]->decreaseTraversalForce();
                    d_children[i]->parentList.remove(this);
                }
                else
                {
                    cerr << "VrmlNodeGroup::setField(children): had NULL child" << endl;
                }
            }

            d_children = (VrmlMFNode &)fieldValue;

            for (int i = 0; i < d_children.size(); i++)
            {
                VrmlNode *child = d_children[i];
                if (child == NULL)
                {
                    continue;
                }

                child->parentList.push_back(this);
                if (child->getTraversalForce() > 0)
                {
                    forceTraversal(false, child->getTraversalForce());
                }
            }
        }
        else
            System::the->error("Invalid type (%s) for %s field of %s node (expected %s).\n",
                               fieldValue.fieldTypeName(), "children", nodeType()->getName(), "MFNode");
    }
    else
        VrmlNodeChild::setField(fieldName, fieldValue);
}

const VrmlField *VrmlNodeGroup::getField(const char *fieldName) const
{
    if (strcmp(fieldName, "bboxCenter") == 0)
        return &d_bboxCenter;
    else if (strcmp(fieldName, "bboxSize") == 0)
        return &d_bboxSize;
    else if (strcmp(fieldName, "children") == 0)
        return &d_children;

    return VrmlNodeChild::getField(fieldName);
}

int VrmlNodeGroup::size()
{
    return d_children.size();
}

VrmlNode *VrmlNodeGroup::child(int index)
{
    if (index >= 0 && index < d_children.size())
        return d_children[index];

    return 0;
}

// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_instancemanager.h"

namespace {

const CXFA_Node::PropertyData kPropertyData[] = {{XFA_Element::Occur, 1, 0},
                                                 {XFA_Element::Unknown, 0, 0}};
const CXFA_Node::AttributeData kAttributeData[] = {
    {XFA_Attribute::Name, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Unknown, XFA_AttributeType::Integer, nullptr}};

constexpr wchar_t kName[] = L"instanceManager";

}  // namespace

CXFA_InstanceManager::CXFA_InstanceManager(CXFA_Document* doc,
                                           XFA_XDPPACKET packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_Form,
                XFA_ObjectType::Node,
                XFA_Element::InstanceManager,
                kPropertyData,
                kAttributeData,
                kName) {}

CXFA_InstanceManager::~CXFA_InstanceManager() {}

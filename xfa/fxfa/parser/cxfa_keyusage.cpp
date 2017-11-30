// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_keyusage.h"

namespace {

const CXFA_Node::AttributeData kAttributeData[] = {
    {XFA_Attribute::Id, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Use, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::NonRepudiation, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::EncipherOnly, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Type, XFA_AttributeType::Enum,
     (void*)XFA_ATTRIBUTEENUM_Optional},
    {XFA_Attribute::DigitalSignature, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::CrlSign, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::KeyAgreement, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::KeyEncipherment, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Usehref, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::DataEncipherment, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::KeyCertSign, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::DecipherOnly, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Unknown, XFA_AttributeType::Integer, nullptr}};

constexpr wchar_t kName[] = L"keyUsage";

}  // namespace

CXFA_KeyUsage::CXFA_KeyUsage(CXFA_Document* doc, XFA_XDPPACKET packet)
    : CXFA_Node(doc,
                packet,
                (XFA_XDPPACKET_Template | XFA_XDPPACKET_Form),
                XFA_ObjectType::Node,
                XFA_Element::KeyUsage,
                nullptr,
                kAttributeData,
                kName) {}

CXFA_KeyUsage::~CXFA_KeyUsage() {}

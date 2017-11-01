// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_node.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "core/fxcrt/cfx_decimal.h"
#include "core/fxcrt/cfx_memorystream.h"
#include "core/fxcrt/fx_codepage.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/xml/cfx_xmlelement.h"
#include "core/fxcrt/xml/cfx_xmlnode.h"
#include "core/fxcrt/xml/cfx_xmltext.h"
#include "fxjs/cfxjse_engine.h"
#include "fxjs/cfxjse_value.h"
#include "third_party/base/logging.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"
#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/cxfa_ffnotify.h"
#include "xfa/fxfa/cxfa_ffwidget.h"
#include "xfa/fxfa/parser/cxfa_arraynodelist.h"
#include "xfa/fxfa/parser/cxfa_attachnodelist.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_layoutprocessor.h"
#include "xfa/fxfa/parser/cxfa_measurement.h"
#include "xfa/fxfa/parser/cxfa_occur.h"
#include "xfa/fxfa/parser/cxfa_simple_parser.h"
#include "xfa/fxfa/parser/cxfa_traversestrategy_xfacontainernode.h"
#include "xfa/fxfa/parser/xfa_basic_data.h"

namespace {

void XFA_DataNodeDeleteBindItem(void* pData) {
  delete static_cast<std::vector<CXFA_Node*>*>(pData);
}

XFA_MAPDATABLOCKCALLBACKINFO deleteBindItemCallBack = {
    XFA_DataNodeDeleteBindItem, nullptr};

std::vector<CXFA_Node*> NodesSortedByDocumentIdx(
    const std::set<CXFA_Node*>& rgNodeSet) {
  if (rgNodeSet.empty())
    return std::vector<CXFA_Node*>();

  std::vector<CXFA_Node*> rgNodeArray;
  CXFA_Node* pCommonParent =
      (*rgNodeSet.begin())->GetNodeItem(XFA_NODEITEM_Parent);
  for (CXFA_Node* pNode = pCommonParent->GetNodeItem(XFA_NODEITEM_FirstChild);
       pNode; pNode = pNode->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    if (pdfium::ContainsValue(rgNodeSet, pNode))
      rgNodeArray.push_back(pNode);
  }
  return rgNodeArray;
}

using CXFA_NodeSetPair = std::pair<std::set<CXFA_Node*>, std::set<CXFA_Node*>>;
using CXFA_NodeSetPairMap =
    std::map<uint32_t, std::unique_ptr<CXFA_NodeSetPair>>;
using CXFA_NodeSetPairMapMap =
    std::map<CXFA_Node*, std::unique_ptr<CXFA_NodeSetPairMap>>;

CXFA_NodeSetPair* NodeSetPairForNode(CXFA_Node* pNode,
                                     CXFA_NodeSetPairMapMap* pMap) {
  CXFA_Node* pParentNode = pNode->GetNodeItem(XFA_NODEITEM_Parent);
  uint32_t dwNameHash = pNode->GetNameHash();
  if (!pParentNode || !dwNameHash)
    return nullptr;

  if (!(*pMap)[pParentNode])
    (*pMap)[pParentNode] = pdfium::MakeUnique<CXFA_NodeSetPairMap>();

  CXFA_NodeSetPairMap* pNodeSetPairMap = (*pMap)[pParentNode].get();
  if (!(*pNodeSetPairMap)[dwNameHash])
    (*pNodeSetPairMap)[dwNameHash] = pdfium::MakeUnique<CXFA_NodeSetPair>();

  return (*pNodeSetPairMap)[dwNameHash].get();
}

void ReorderDataNodes(const std::set<CXFA_Node*>& sSet1,
                      const std::set<CXFA_Node*>& sSet2,
                      bool bInsertBefore) {
  CXFA_NodeSetPairMapMap rgMap;
  for (CXFA_Node* pNode : sSet1) {
    CXFA_NodeSetPair* pNodeSetPair = NodeSetPairForNode(pNode, &rgMap);
    if (pNodeSetPair)
      pNodeSetPair->first.insert(pNode);
  }
  for (CXFA_Node* pNode : sSet2) {
    CXFA_NodeSetPair* pNodeSetPair = NodeSetPairForNode(pNode, &rgMap);
    if (pNodeSetPair) {
      if (pdfium::ContainsValue(pNodeSetPair->first, pNode))
        pNodeSetPair->first.erase(pNode);
      else
        pNodeSetPair->second.insert(pNode);
    }
  }
  for (const auto& iter1 : rgMap) {
    CXFA_NodeSetPairMap* pNodeSetPairMap = iter1.second.get();
    if (!pNodeSetPairMap)
      continue;

    for (const auto& iter2 : *pNodeSetPairMap) {
      CXFA_NodeSetPair* pNodeSetPair = iter2.second.get();
      if (!pNodeSetPair)
        continue;
      if (!pNodeSetPair->first.empty() && !pNodeSetPair->second.empty()) {
        std::vector<CXFA_Node*> rgNodeArray1 =
            NodesSortedByDocumentIdx(pNodeSetPair->first);
        std::vector<CXFA_Node*> rgNodeArray2 =
            NodesSortedByDocumentIdx(pNodeSetPair->second);
        CXFA_Node* pParentNode = nullptr;
        CXFA_Node* pBeforeNode = nullptr;
        if (bInsertBefore) {
          pBeforeNode = rgNodeArray2.front();
          pParentNode = pBeforeNode->GetNodeItem(XFA_NODEITEM_Parent);
        } else {
          CXFA_Node* pLastNode = rgNodeArray2.back();
          pParentNode = pLastNode->GetNodeItem(XFA_NODEITEM_Parent);
          pBeforeNode = pLastNode->GetNodeItem(XFA_NODEITEM_NextSibling);
        }
        for (auto* pCurNode : rgNodeArray1) {
          pParentNode->RemoveChild(pCurNode);
          pParentNode->InsertChild(pCurNode, pBeforeNode);
        }
      }
    }
    pNodeSetPairMap->clear();
  }
}

}  // namespace

const XFA_ATTRIBUTEENUMINFO* GetAttributeEnumByID(XFA_ATTRIBUTEENUM eName) {
  return g_XFAEnumData + eName;
}

CXFA_Node::CXFA_Node(CXFA_Document* pDoc,
                     uint16_t ePacket,
                     XFA_ObjectType oType,
                     XFA_Element eType,
                     const WideStringView& elementName)
    : CXFA_Object(pDoc,
                  oType,
                  eType,
                  elementName,
                  pdfium::MakeUnique<CJX_Node>(this)),
      m_pNext(nullptr),
      m_pChild(nullptr),
      m_pLastChild(nullptr),
      m_pParent(nullptr),
      m_pXMLNode(nullptr),
      m_ePacket(ePacket),
      m_uNodeFlags(XFA_NodeFlag_None),
      m_dwNameHash(0),
      m_pAuxNode(nullptr) {
  ASSERT(m_pDocument);
}

CXFA_Node::~CXFA_Node() {
  ASSERT(!m_pParent);
  CXFA_Node* pNode = m_pChild;
  while (pNode) {
    CXFA_Node* pNext = pNode->m_pNext;
    pNode->m_pParent = nullptr;
    delete pNode;
    pNode = pNext;
  }
  if (m_pXMLNode && IsOwnXMLNode())
    delete m_pXMLNode;
}

CXFA_Node* CXFA_Node::Clone(bool bRecursive) {
  CXFA_Node* pClone = m_pDocument->CreateNode(m_ePacket, m_elementType);
  if (!pClone)
    return nullptr;

  JSNode()->MergeAllData(pClone);
  pClone->UpdateNameHash();
  if (IsNeedSavingXMLNode()) {
    std::unique_ptr<CFX_XMLNode> pCloneXML;
    if (IsAttributeInXML()) {
      WideString wsName;
      JSNode()->GetAttribute(XFA_ATTRIBUTE_Name, wsName, false);
      auto pCloneXMLElement = pdfium::MakeUnique<CFX_XMLElement>(wsName);
      WideStringView wsValue = JSNode()->GetCData(XFA_ATTRIBUTE_Value);
      if (!wsValue.IsEmpty()) {
        pCloneXMLElement->SetTextData(WideString(wsValue));
      }
      pCloneXML.reset(pCloneXMLElement.release());
      pClone->JSNode()->SetEnum(XFA_ATTRIBUTE_Contains,
                                XFA_ATTRIBUTEENUM_Unknown);
    } else {
      pCloneXML = m_pXMLNode->Clone();
    }
    pClone->SetXMLMappingNode(pCloneXML.release());
    pClone->SetFlag(XFA_NodeFlag_OwnXMLNode, false);
  }
  if (bRecursive) {
    for (CXFA_Node* pChild = GetNodeItem(XFA_NODEITEM_FirstChild); pChild;
         pChild = pChild->GetNodeItem(XFA_NODEITEM_NextSibling)) {
      pClone->InsertChild(pChild->Clone(bRecursive));
    }
  }
  pClone->SetFlag(XFA_NodeFlag_Initialized, true);
  pClone->JSNode()->SetObject(XFA_ATTRIBUTE_BindingNode, nullptr);
  return pClone;
}

CXFA_Node* CXFA_Node::GetNodeItem(XFA_NODEITEM eItem) const {
  switch (eItem) {
    case XFA_NODEITEM_NextSibling:
      return m_pNext;
    case XFA_NODEITEM_FirstChild:
      return m_pChild;
    case XFA_NODEITEM_Parent:
      return m_pParent;
    case XFA_NODEITEM_PrevSibling:
      if (m_pParent) {
        CXFA_Node* pSibling = m_pParent->m_pChild;
        CXFA_Node* pPrev = nullptr;
        while (pSibling && pSibling != this) {
          pPrev = pSibling;
          pSibling = pSibling->m_pNext;
        }
        return pPrev;
      }
      return nullptr;
    default:
      break;
  }
  return nullptr;
}

CXFA_Node* CXFA_Node::GetNodeItem(XFA_NODEITEM eItem,
                                  XFA_ObjectType eType) const {
  CXFA_Node* pNode = nullptr;
  switch (eItem) {
    case XFA_NODEITEM_NextSibling:
      pNode = m_pNext;
      while (pNode && pNode->GetObjectType() != eType)
        pNode = pNode->m_pNext;
      break;
    case XFA_NODEITEM_FirstChild:
      pNode = m_pChild;
      while (pNode && pNode->GetObjectType() != eType)
        pNode = pNode->m_pNext;
      break;
    case XFA_NODEITEM_Parent:
      pNode = m_pParent;
      while (pNode && pNode->GetObjectType() != eType)
        pNode = pNode->m_pParent;
      break;
    case XFA_NODEITEM_PrevSibling:
      if (m_pParent) {
        CXFA_Node* pSibling = m_pParent->m_pChild;
        while (pSibling && pSibling != this) {
          if (eType == pSibling->GetObjectType())
            pNode = pSibling;

          pSibling = pSibling->m_pNext;
        }
      }
      break;
    default:
      break;
  }
  return pNode;
}

std::vector<CXFA_Node*> CXFA_Node::GetNodeList(uint32_t dwTypeFilter,
                                               XFA_Element eTypeFilter) {
  std::vector<CXFA_Node*> nodes;
  if (eTypeFilter != XFA_Element::Unknown) {
    for (CXFA_Node* pChild = m_pChild; pChild; pChild = pChild->m_pNext) {
      if (pChild->GetElementType() == eTypeFilter)
        nodes.push_back(pChild);
    }
  } else if (dwTypeFilter ==
             (XFA_NODEFILTER_Children | XFA_NODEFILTER_Properties)) {
    for (CXFA_Node* pChild = m_pChild; pChild; pChild = pChild->m_pNext)
      nodes.push_back(pChild);
  } else if (dwTypeFilter != 0) {
    bool bFilterChildren = !!(dwTypeFilter & XFA_NODEFILTER_Children);
    bool bFilterProperties = !!(dwTypeFilter & XFA_NODEFILTER_Properties);
    bool bFilterOneOfProperties =
        !!(dwTypeFilter & XFA_NODEFILTER_OneOfProperty);
    CXFA_Node* pChild = m_pChild;
    while (pChild) {
      const XFA_PROPERTY* pProperty = XFA_GetPropertyOfElement(
          GetElementType(), pChild->GetElementType(), XFA_XDPPACKET_UNKNOWN);
      if (pProperty) {
        if (bFilterProperties) {
          nodes.push_back(pChild);
        } else if (bFilterOneOfProperties &&
                   (pProperty->uFlags & XFA_PROPERTYFLAG_OneOf)) {
          nodes.push_back(pChild);
        } else if (bFilterChildren &&
                   (pChild->GetElementType() == XFA_Element::Variables ||
                    pChild->GetElementType() == XFA_Element::PageSet)) {
          nodes.push_back(pChild);
        }
      } else if (bFilterChildren) {
        nodes.push_back(pChild);
      }
      pChild = pChild->m_pNext;
    }
    if (bFilterOneOfProperties && nodes.empty()) {
      int32_t iProperties = 0;
      const XFA_PROPERTY* pProperty =
          XFA_GetElementProperties(GetElementType(), iProperties);
      if (!pProperty || iProperties < 1)
        return nodes;
      for (int32_t i = 0; i < iProperties; i++) {
        if (pProperty[i].uFlags & XFA_PROPERTYFLAG_DefaultOneOf) {
          const XFA_PACKETINFO* pPacket = XFA_GetPacketByID(GetPacketID());
          CXFA_Node* pNewNode =
              m_pDocument->CreateNode(pPacket, pProperty[i].eName);
          if (!pNewNode)
            break;
          InsertChild(pNewNode, nullptr);
          pNewNode->SetFlag(XFA_NodeFlag_Initialized, true);
          nodes.push_back(pNewNode);
          break;
        }
      }
    }
  }
  return nodes;
}

CXFA_Node* CXFA_Node::CreateSamePacketNode(XFA_Element eType,
                                           uint32_t dwFlags) {
  CXFA_Node* pNode = m_pDocument->CreateNode(m_ePacket, eType);
  pNode->SetFlag(dwFlags, true);
  return pNode;
}

CXFA_Node* CXFA_Node::CloneTemplateToForm(bool bRecursive) {
  ASSERT(m_ePacket == XFA_XDPPACKET_Template);
  CXFA_Node* pClone =
      m_pDocument->CreateNode(XFA_XDPPACKET_Form, m_elementType);
  if (!pClone)
    return nullptr;

  pClone->SetTemplateNode(this);
  pClone->UpdateNameHash();
  pClone->SetXMLMappingNode(GetXMLMappingNode());
  if (bRecursive) {
    for (CXFA_Node* pChild = GetNodeItem(XFA_NODEITEM_FirstChild); pChild;
         pChild = pChild->GetNodeItem(XFA_NODEITEM_NextSibling)) {
      pClone->InsertChild(pChild->CloneTemplateToForm(bRecursive));
    }
  }
  pClone->SetFlag(XFA_NodeFlag_Initialized, true);
  return pClone;
}

CXFA_Node* CXFA_Node::GetTemplateNode() const {
  return m_pAuxNode;
}

void CXFA_Node::SetTemplateNode(CXFA_Node* pTemplateNode) {
  m_pAuxNode = pTemplateNode;
}

CXFA_Node* CXFA_Node::GetBindData() {
  ASSERT(GetPacketID() == XFA_XDPPACKET_Form);
  return static_cast<CXFA_Node*>(
      JSNode()->GetObject(XFA_ATTRIBUTE_BindingNode));
}

std::vector<CXFA_Node*> CXFA_Node::GetBindItems() {
  if (BindsFormItems()) {
    void* pBinding = nullptr;
    JSNode()->TryObject(XFA_ATTRIBUTE_BindingNode, pBinding);
    return *static_cast<std::vector<CXFA_Node*>*>(pBinding);
  }
  std::vector<CXFA_Node*> result;
  CXFA_Node* pFormNode =
      static_cast<CXFA_Node*>(JSNode()->GetObject(XFA_ATTRIBUTE_BindingNode));
  if (pFormNode)
    result.push_back(pFormNode);
  return result;
}

int32_t CXFA_Node::AddBindItem(CXFA_Node* pFormNode) {
  ASSERT(pFormNode);
  if (BindsFormItems()) {
    void* pBinding = nullptr;
    JSNode()->TryObject(XFA_ATTRIBUTE_BindingNode, pBinding);
    auto* pItems = static_cast<std::vector<CXFA_Node*>*>(pBinding);
    if (!pdfium::ContainsValue(*pItems, pFormNode))
      pItems->push_back(pFormNode);
    return pdfium::CollectionSize<int32_t>(*pItems);
  }
  CXFA_Node* pOldFormItem =
      static_cast<CXFA_Node*>(JSNode()->GetObject(XFA_ATTRIBUTE_BindingNode));
  if (!pOldFormItem) {
    JSNode()->SetObject(XFA_ATTRIBUTE_BindingNode, pFormNode);
    return 1;
  }
  if (pOldFormItem == pFormNode)
    return 1;

  std::vector<CXFA_Node*>* pItems = new std::vector<CXFA_Node*>;
  JSNode()->SetObject(XFA_ATTRIBUTE_BindingNode, pItems,
                      &deleteBindItemCallBack);
  pItems->push_back(pOldFormItem);
  pItems->push_back(pFormNode);
  m_uNodeFlags |= XFA_NodeFlag_BindFormItems;
  return 2;
}

int32_t CXFA_Node::RemoveBindItem(CXFA_Node* pFormNode) {
  if (BindsFormItems()) {
    void* pBinding = nullptr;
    JSNode()->TryObject(XFA_ATTRIBUTE_BindingNode, pBinding);
    auto* pItems = static_cast<std::vector<CXFA_Node*>*>(pBinding);
    auto iter = std::find(pItems->begin(), pItems->end(), pFormNode);
    if (iter != pItems->end()) {
      *iter = pItems->back();
      pItems->pop_back();
      if (pItems->size() == 1) {
        JSNode()->SetObject(XFA_ATTRIBUTE_BindingNode,
                            (*pItems)[0]);  // Invalidates pItems.
        m_uNodeFlags &= ~XFA_NodeFlag_BindFormItems;
        return 1;
      }
    }
    return pdfium::CollectionSize<int32_t>(*pItems);
  }
  CXFA_Node* pOldFormItem =
      static_cast<CXFA_Node*>(JSNode()->GetObject(XFA_ATTRIBUTE_BindingNode));
  if (pOldFormItem != pFormNode)
    return pOldFormItem ? 1 : 0;

  JSNode()->SetObject(XFA_ATTRIBUTE_BindingNode, nullptr);
  return 0;
}

bool CXFA_Node::HasBindItem() {
  return GetPacketID() == XFA_XDPPACKET_Datasets &&
         JSNode()->GetObject(XFA_ATTRIBUTE_BindingNode);
}

CXFA_WidgetData* CXFA_Node::GetWidgetData() {
  return (CXFA_WidgetData*)JSNode()->GetObject(XFA_ATTRIBUTE_WidgetData);
}

CXFA_WidgetData* CXFA_Node::GetContainerWidgetData() {
  if (GetPacketID() != XFA_XDPPACKET_Form)
    return nullptr;
  XFA_Element eType = GetElementType();
  if (eType == XFA_Element::ExclGroup)
    return nullptr;
  CXFA_Node* pParentNode = GetNodeItem(XFA_NODEITEM_Parent);
  if (pParentNode && pParentNode->GetElementType() == XFA_Element::ExclGroup)
    return nullptr;

  if (eType == XFA_Element::Field) {
    CXFA_WidgetData* pFieldWidgetData = GetWidgetData();
    if (pFieldWidgetData &&
        pFieldWidgetData->GetChoiceListOpen() ==
            XFA_ATTRIBUTEENUM_MultiSelect) {
      return nullptr;
    } else {
      WideString wsPicture;
      if (pFieldWidgetData) {
        pFieldWidgetData->GetPictureContent(wsPicture,
                                            XFA_VALUEPICTURE_DataBind);
      }
      if (!wsPicture.IsEmpty())
        return pFieldWidgetData;
      CXFA_Node* pDataNode = GetBindData();
      if (!pDataNode)
        return nullptr;
      pFieldWidgetData = nullptr;
      for (CXFA_Node* pFormNode : pDataNode->GetBindItems()) {
        if (!pFormNode || pFormNode->HasRemovedChildren())
          continue;
        pFieldWidgetData = pFormNode->GetWidgetData();
        if (pFieldWidgetData) {
          pFieldWidgetData->GetPictureContent(wsPicture,
                                              XFA_VALUEPICTURE_DataBind);
        }
        if (!wsPicture.IsEmpty())
          break;
        pFieldWidgetData = nullptr;
      }
      return pFieldWidgetData;
    }
  }
  CXFA_Node* pGrandNode =
      pParentNode ? pParentNode->GetNodeItem(XFA_NODEITEM_Parent) : nullptr;
  CXFA_Node* pValueNode =
      (pParentNode && pParentNode->GetElementType() == XFA_Element::Value)
          ? pParentNode
          : nullptr;
  if (!pValueNode) {
    pValueNode =
        (pGrandNode && pGrandNode->GetElementType() == XFA_Element::Value)
            ? pGrandNode
            : nullptr;
  }
  CXFA_Node* pParentOfValueNode =
      pValueNode ? pValueNode->GetNodeItem(XFA_NODEITEM_Parent) : nullptr;
  return pParentOfValueNode ? pParentOfValueNode->GetContainerWidgetData()
                            : nullptr;
}

bool CXFA_Node::GetLocaleName(WideString& wsLocaleName) {
  CXFA_Node* pForm = GetDocument()->GetXFAObject(XFA_HASHCODE_Form)->AsNode();
  CXFA_Node* pTopSubform = pForm->GetFirstChildByClass(XFA_Element::Subform);
  ASSERT(pTopSubform);
  CXFA_Node* pLocaleNode = this;
  bool bLocale = false;
  do {
    bLocale = pLocaleNode->JSNode()->TryCData(XFA_ATTRIBUTE_Locale,
                                              wsLocaleName, false);
    if (!bLocale) {
      pLocaleNode = pLocaleNode->GetNodeItem(XFA_NODEITEM_Parent);
    }
  } while (pLocaleNode && pLocaleNode != pTopSubform && !bLocale);
  if (bLocale)
    return true;
  CXFA_Node* pConfig = ToNode(GetDocument()->GetXFAObject(XFA_HASHCODE_Config));
  wsLocaleName = GetDocument()->GetLocalMgr()->GetConfigLocaleName(pConfig);
  if (!wsLocaleName.IsEmpty())
    return true;
  if (pTopSubform && pTopSubform->JSNode()->TryCData(XFA_ATTRIBUTE_Locale,
                                                     wsLocaleName, false)) {
    return true;
  }
  IFX_Locale* pLocale = GetDocument()->GetLocalMgr()->GetDefLocale();
  if (pLocale) {
    wsLocaleName = pLocale->GetName();
    return true;
  }
  return false;
}

XFA_ATTRIBUTEENUM CXFA_Node::GetIntact() {
  CXFA_Node* pKeep = GetFirstChildByClass(XFA_Element::Keep);
  XFA_ATTRIBUTEENUM eLayoutType = JSNode()->GetEnum(XFA_ATTRIBUTE_Layout);
  if (pKeep) {
    XFA_ATTRIBUTEENUM eIntact;
    if (pKeep->JSNode()->TryEnum(XFA_ATTRIBUTE_Intact, eIntact, false)) {
      if (eIntact == XFA_ATTRIBUTEENUM_None &&
          eLayoutType == XFA_ATTRIBUTEENUM_Row &&
          m_pDocument->GetCurVersionMode() < XFA_VERSION_208) {
        CXFA_Node* pPreviewRow = GetNodeItem(XFA_NODEITEM_PrevSibling,
                                             XFA_ObjectType::ContainerNode);
        if (pPreviewRow && pPreviewRow->JSNode()->GetEnum(
                               XFA_ATTRIBUTE_Layout) == XFA_ATTRIBUTEENUM_Row) {
          XFA_ATTRIBUTEENUM eValue;
          if (pKeep->JSNode()->TryEnum(XFA_ATTRIBUTE_Previous, eValue, false) &&
              (eValue == XFA_ATTRIBUTEENUM_ContentArea ||
               eValue == XFA_ATTRIBUTEENUM_PageArea)) {
            return XFA_ATTRIBUTEENUM_ContentArea;
          }
          CXFA_Node* pNode =
              pPreviewRow->GetFirstChildByClass(XFA_Element::Keep);
          if (pNode &&
              pNode->JSNode()->TryEnum(XFA_ATTRIBUTE_Next, eValue, false) &&
              (eValue == XFA_ATTRIBUTEENUM_ContentArea ||
               eValue == XFA_ATTRIBUTEENUM_PageArea)) {
            return XFA_ATTRIBUTEENUM_ContentArea;
          }
        }
      }
      return eIntact;
    }
  }
  switch (GetElementType()) {
    case XFA_Element::Subform:
      switch (eLayoutType) {
        case XFA_ATTRIBUTEENUM_Position:
        case XFA_ATTRIBUTEENUM_Row:
          return XFA_ATTRIBUTEENUM_ContentArea;
        case XFA_ATTRIBUTEENUM_Tb:
        case XFA_ATTRIBUTEENUM_Table:
        case XFA_ATTRIBUTEENUM_Lr_tb:
        case XFA_ATTRIBUTEENUM_Rl_tb:
          return XFA_ATTRIBUTEENUM_None;
        default:
          break;
      }
      break;
    case XFA_Element::Field: {
      CXFA_Node* pParentNode = GetNodeItem(XFA_NODEITEM_Parent);
      if (!pParentNode ||
          pParentNode->GetElementType() == XFA_Element::PageArea)
        return XFA_ATTRIBUTEENUM_ContentArea;
      if (pParentNode->GetIntact() == XFA_ATTRIBUTEENUM_None) {
        XFA_ATTRIBUTEENUM eParLayout =
            pParentNode->JSNode()->GetEnum(XFA_ATTRIBUTE_Layout);
        if (eParLayout == XFA_ATTRIBUTEENUM_Position ||
            eParLayout == XFA_ATTRIBUTEENUM_Row ||
            eParLayout == XFA_ATTRIBUTEENUM_Table) {
          return XFA_ATTRIBUTEENUM_None;
        }
        XFA_VERSION version = m_pDocument->GetCurVersionMode();
        if (eParLayout == XFA_ATTRIBUTEENUM_Tb && version < XFA_VERSION_208) {
          CXFA_Measurement measureH;
          if (JSNode()->TryMeasure(XFA_ATTRIBUTE_H, measureH, false))
            return XFA_ATTRIBUTEENUM_ContentArea;
        }
        return XFA_ATTRIBUTEENUM_None;
      }
      return XFA_ATTRIBUTEENUM_ContentArea;
    }
    case XFA_Element::Draw:
      return XFA_ATTRIBUTEENUM_ContentArea;
    default:
      break;
  }
  return XFA_ATTRIBUTEENUM_None;
}

CXFA_Node* CXFA_Node::GetDataDescriptionNode() {
  if (m_ePacket == XFA_XDPPACKET_Datasets)
    return m_pAuxNode;
  return nullptr;
}

void CXFA_Node::SetDataDescriptionNode(CXFA_Node* pDataDescriptionNode) {
  ASSERT(m_ePacket == XFA_XDPPACKET_Datasets);
  m_pAuxNode = pDataDescriptionNode;
}

void CXFA_Node::Script_TreeClass_ResolveNode(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_TreeClass_ResolveNode(pArguments);
}

void CXFA_Node::Script_TreeClass_ResolveNodes(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_TreeClass_ResolveNodes(pArguments);
}

void CXFA_Node::Script_Som_ResolveNodeList(CFXJSE_Value* pValue,
                                           WideString wsExpression,
                                           uint32_t dwFlag,
                                           CXFA_Node* refNode) {
  JSNode()->Script_Som_ResolveNodeList(pValue, wsExpression, dwFlag, refNode);
}

void CXFA_Node::Script_TreeClass_All(CFXJSE_Value* pValue,
                                     bool bSetting,
                                     XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_TreeClass_All(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_TreeClass_Nodes(CFXJSE_Value* pValue,
                                       bool bSetting,
                                       XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_TreeClass_Nodes(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_TreeClass_ClassAll(CFXJSE_Value* pValue,
                                          bool bSetting,
                                          XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_TreeClass_ClassAll(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_TreeClass_Parent(CFXJSE_Value* pValue,
                                        bool bSetting,
                                        XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_TreeClass_Parent(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_TreeClass_Index(CFXJSE_Value* pValue,
                                       bool bSetting,
                                       XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_TreeClass_Index(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_TreeClass_ClassIndex(CFXJSE_Value* pValue,
                                            bool bSetting,
                                            XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_TreeClass_ClassIndex(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_TreeClass_SomExpression(CFXJSE_Value* pValue,
                                               bool bSetting,
                                               XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_TreeClass_SomExpression(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_NodeClass_ApplyXSL(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_NodeClass_ApplyXSL(pArguments);
}

void CXFA_Node::Script_NodeClass_AssignNode(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_NodeClass_AssignNode(pArguments);
}

void CXFA_Node::Script_NodeClass_Clone(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_NodeClass_Clone(pArguments);
}

void CXFA_Node::Script_NodeClass_GetAttribute(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_NodeClass_GetAttribute(pArguments);
}

void CXFA_Node::Script_NodeClass_GetElement(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_NodeClass_GetElement(pArguments);
}

void CXFA_Node::Script_NodeClass_IsPropertySpecified(
    CFXJSE_Arguments* pArguments) {
  JSNode()->Script_NodeClass_IsPropertySpecified(pArguments);
}

void CXFA_Node::Script_NodeClass_LoadXML(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_NodeClass_LoadXML(pArguments);
}

void CXFA_Node::Script_NodeClass_SaveFilteredXML(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_NodeClass_SaveFilteredXML(pArguments);
}

void CXFA_Node::Script_NodeClass_SaveXML(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_NodeClass_SaveXML(pArguments);
}

void CXFA_Node::Script_NodeClass_SetAttribute(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_NodeClass_SetAttribute(pArguments);
}

void CXFA_Node::Script_NodeClass_SetElement(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_NodeClass_SetElement(pArguments);
}

void CXFA_Node::Script_NodeClass_Ns(CFXJSE_Value* pValue,
                                    bool bSetting,
                                    XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_NodeClass_Ns(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_NodeClass_Model(CFXJSE_Value* pValue,
                                       bool bSetting,
                                       XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_NodeClass_Model(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_NodeClass_IsContainer(CFXJSE_Value* pValue,
                                             bool bSetting,
                                             XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_NodeClass_IsContainer(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_NodeClass_IsNull(CFXJSE_Value* pValue,
                                        bool bSetting,
                                        XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_NodeClass_IsNull(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_NodeClass_OneOfChild(CFXJSE_Value* pValue,
                                            bool bSetting,
                                            XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_NodeClass_OneOfChild(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_ContainerClass_GetDelta(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_ContainerClass_GetDelta(pArguments);
}

void CXFA_Node::Script_ContainerClass_GetDeltas(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_ContainerClass_GetDeltas(pArguments);
}

void CXFA_Node::Script_ModelClass_ClearErrorList(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_ModelClass_ClearErrorList(pArguments);
}

void CXFA_Node::Script_ModelClass_CreateNode(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_ModelClass_CreateNode(pArguments);
}

void CXFA_Node::Script_ModelClass_IsCompatibleNS(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_ModelClass_IsCompatibleNS(pArguments);
}

void CXFA_Node::Script_ModelClass_Context(CFXJSE_Value* pValue,
                                          bool bSetting,
                                          XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_ModelClass_Context(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_ModelClass_AliasNode(CFXJSE_Value* pValue,
                                            bool bSetting,
                                            XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_ModelClass_AliasNode(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Attribute_Integer(CFXJSE_Value* pValue,
                                         bool bSetting,
                                         XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Attribute_Integer(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Attribute_IntegerRead(CFXJSE_Value* pValue,
                                             bool bSetting,
                                             XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Attribute_IntegerRead(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Attribute_BOOL(CFXJSE_Value* pValue,
                                      bool bSetting,
                                      XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Attribute_BOOL(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Attribute_BOOLRead(CFXJSE_Value* pValue,
                                          bool bSetting,
                                          XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Attribute_BOOLRead(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Attribute_SendAttributeChangeMessage(
    XFA_ATTRIBUTE eAttribute,
    bool bScriptModify) {
  JSNode()->Script_Attribute_SendAttributeChangeMessage(eAttribute,
                                                        bScriptModify);
}

void CXFA_Node::Script_Attribute_String(CFXJSE_Value* pValue,
                                        bool bSetting,
                                        XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Attribute_StringRead(CFXJSE_Value* pValue,
                                            bool bSetting,
                                            XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Attribute_StringRead(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_WsdlConnection_Execute(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_WsdlConnection_Execute(pArguments);
}

void CXFA_Node::Script_Delta_Restore(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Delta_Restore(pArguments);
}

void CXFA_Node::Script_Delta_CurrentValue(CFXJSE_Value* pValue,
                                          bool bSetting,
                                          XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Delta_CurrentValue(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Delta_SavedValue(CFXJSE_Value* pValue,
                                        bool bSetting,
                                        XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Delta_SavedValue(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Delta_Target(CFXJSE_Value* pValue,
                                    bool bSetting,
                                    XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Delta_Target(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Som_Message(CFXJSE_Value* pValue,
                                   bool bSetting,
                                   XFA_SOM_MESSAGETYPE iMessageType) {
  JSNode()->Script_Som_Message(pValue, bSetting, iMessageType);
}

void CXFA_Node::Script_Som_ValidationMessage(CFXJSE_Value* pValue,
                                             bool bSetting,
                                             XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Som_ValidationMessage(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Field_Length(CFXJSE_Value* pValue,
                                    bool bSetting,
                                    XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Field_Length(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Som_DefaultValue(CFXJSE_Value* pValue,
                                        bool bSetting,
                                        XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Som_DefaultValue(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Som_DefaultValue_Read(CFXJSE_Value* pValue,
                                             bool bSetting,
                                             XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Som_DefaultValue_Read(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Boolean_Value(CFXJSE_Value* pValue,
                                     bool bSetting,
                                     XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Boolean_Value(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Som_BorderColor(CFXJSE_Value* pValue,
                                       bool bSetting,
                                       XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Som_BorderColor(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Som_BorderWidth(CFXJSE_Value* pValue,
                                       bool bSetting,
                                       XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Som_BorderWidth(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Som_FillColor(CFXJSE_Value* pValue,
                                     bool bSetting,
                                     XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Som_FillColor(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Som_DataNode(CFXJSE_Value* pValue,
                                    bool bSetting,
                                    XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Som_DataNode(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Draw_DefaultValue(CFXJSE_Value* pValue,
                                         bool bSetting,
                                         XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Draw_DefaultValue(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Field_DefaultValue(CFXJSE_Value* pValue,
                                          bool bSetting,
                                          XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Field_DefaultValue(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Field_EditValue(CFXJSE_Value* pValue,
                                       bool bSetting,
                                       XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Field_EditValue(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Som_FontColor(CFXJSE_Value* pValue,
                                     bool bSetting,
                                     XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Som_FontColor(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Field_FormatMessage(CFXJSE_Value* pValue,
                                           bool bSetting,
                                           XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Field_FormatMessage(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Field_FormattedValue(CFXJSE_Value* pValue,
                                            bool bSetting,
                                            XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Field_FormattedValue(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Som_Mandatory(CFXJSE_Value* pValue,
                                     bool bSetting,
                                     XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Som_Mandatory(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Som_MandatoryMessage(CFXJSE_Value* pValue,
                                            bool bSetting,
                                            XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Som_MandatoryMessage(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Field_ParentSubform(CFXJSE_Value* pValue,
                                           bool bSetting,
                                           XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Field_ParentSubform(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Field_SelectedIndex(CFXJSE_Value* pValue,
                                           bool bSetting,
                                           XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Field_SelectedIndex(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Field_ClearItems(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_ClearItems(pArguments);
}

void CXFA_Node::Script_Field_ExecEvent(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_ExecEvent(pArguments);
}

void CXFA_Node::Script_Field_ExecInitialize(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_ExecInitialize(pArguments);
}

void CXFA_Node::Script_Field_DeleteItem(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_DeleteItem(pArguments);
}

void CXFA_Node::Script_Field_GetSaveItem(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_GetSaveItem(pArguments);
}

void CXFA_Node::Script_Field_BoundItem(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_BoundItem(pArguments);
}

void CXFA_Node::Script_Field_GetItemState(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_GetItemState(pArguments);
}

void CXFA_Node::Script_Field_ExecCalculate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_ExecCalculate(pArguments);
}

void CXFA_Node::Script_Field_SetItems(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_SetItems(pArguments);
}

void CXFA_Node::Script_Field_GetDisplayItem(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_GetDisplayItem(pArguments);
}

void CXFA_Node::Script_Field_SetItemState(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_SetItemState(pArguments);
}

void CXFA_Node::Script_Field_AddItem(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_AddItem(pArguments);
}

void CXFA_Node::Script_Field_ExecValidate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Field_ExecValidate(pArguments);
}

void CXFA_Node::Script_ExclGroup_ErrorText(CFXJSE_Value* pValue,
                                           bool bSetting,
                                           XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_ExclGroup_ErrorText(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_ExclGroup_DefaultAndRawValue(CFXJSE_Value* pValue,
                                                    bool bSetting,
                                                    XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_ExclGroup_DefaultAndRawValue(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_ExclGroup_Transient(CFXJSE_Value* pValue,
                                           bool bSetting,
                                           XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_ExclGroup_Transient(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_ExclGroup_ExecEvent(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_ExclGroup_ExecEvent(pArguments);
}

void CXFA_Node::Script_ExclGroup_SelectedMember(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_ExclGroup_SelectedMember(pArguments);
}

void CXFA_Node::Script_ExclGroup_ExecInitialize(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_ExclGroup_ExecInitialize(pArguments);
}

void CXFA_Node::Script_ExclGroup_ExecCalculate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_ExclGroup_ExecCalculate(pArguments);
}

void CXFA_Node::Script_ExclGroup_ExecValidate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_ExclGroup_ExecValidate(pArguments);
}

void CXFA_Node::Script_Som_InstanceIndex(CFXJSE_Value* pValue,
                                         bool bSetting,
                                         XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Som_InstanceIndex(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Subform_InstanceManager(CFXJSE_Value* pValue,
                                               bool bSetting,
                                               XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Subform_InstanceManager(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Subform_Locale(CFXJSE_Value* pValue,
                                      bool bSetting,
                                      XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Subform_Locale(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Subform_ExecEvent(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Subform_ExecEvent(pArguments);
}

void CXFA_Node::Script_Subform_ExecInitialize(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Subform_ExecInitialize(pArguments);
}

void CXFA_Node::Script_Subform_ExecCalculate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Subform_ExecInitialize(pArguments);
}

void CXFA_Node::Script_Subform_ExecValidate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Subform_ExecValidate(pArguments);
}

void CXFA_Node::Script_Subform_GetInvalidObjects(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Subform_GetInvalidObjects(pArguments);
}

void CXFA_Node::Script_Template_FormNodes(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Template_FormNodes(pArguments);
}

void CXFA_Node::Script_Template_Remerge(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Template_Remerge(pArguments);
}

void CXFA_Node::Script_Template_ExecInitialize(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Template_ExecInitialize(pArguments);
}

void CXFA_Node::Script_Template_CreateNode(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Template_CreateNode(pArguments);
}

void CXFA_Node::Script_Template_Recalculate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Template_Recalculate(pArguments);
}

void CXFA_Node::Script_Template_ExecCalculate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Template_ExecCalculate(pArguments);
}

void CXFA_Node::Script_Template_ExecValidate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Template_ExecValidate(pArguments);
}

void CXFA_Node::Script_Manifest_Evaluate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Manifest_Evaluate(pArguments);
}

void CXFA_Node::Script_InstanceManager_Max(CFXJSE_Value* pValue,
                                           bool bSetting,
                                           XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_InstanceManager_Max(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_InstanceManager_Min(CFXJSE_Value* pValue,
                                           bool bSetting,
                                           XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_InstanceManager_Min(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_InstanceManager_Count(CFXJSE_Value* pValue,
                                             bool bSetting,
                                             XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_InstanceManager_Count(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_InstanceManager_MoveInstance(
    CFXJSE_Arguments* pArguments) {
  JSNode()->Script_InstanceManager_MoveInstance(pArguments);
}

void CXFA_Node::Script_InstanceManager_RemoveInstance(
    CFXJSE_Arguments* pArguments) {
  JSNode()->Script_InstanceManager_RemoveInstance(pArguments);
}

void CXFA_Node::Script_InstanceManager_SetInstances(
    CFXJSE_Arguments* pArguments) {
  JSNode()->Script_InstanceManager_SetInstances(pArguments);
}

void CXFA_Node::Script_InstanceManager_AddInstance(
    CFXJSE_Arguments* pArguments) {
  JSNode()->Script_InstanceManager_AddInstance(pArguments);
}

void CXFA_Node::Script_InstanceManager_InsertInstance(
    CFXJSE_Arguments* pArguments) {
  JSNode()->Script_InstanceManager_InsertInstance(pArguments);
}

void CXFA_Node::Script_Occur_Max(CFXJSE_Value* pValue,
                                 bool bSetting,
                                 XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Occur_Max(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Occur_Min(CFXJSE_Value* pValue,
                                 bool bSetting,
                                 XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Occur_Min(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Desc_Metadata(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Desc_Metadata(pArguments);
}

void CXFA_Node::Script_Form_FormNodes(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Form_FormNodes(pArguments);
}

void CXFA_Node::Script_Form_Remerge(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Form_Remerge(pArguments);
}

void CXFA_Node::Script_Form_ExecInitialize(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Form_ExecInitialize(pArguments);
}

void CXFA_Node::Script_Form_Recalculate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Form_Recalculate(pArguments);
}

void CXFA_Node::Script_Form_ExecCalculate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Form_ExecCalculate(pArguments);
}

void CXFA_Node::Script_Form_ExecValidate(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Form_ExecValidate(pArguments);
}

void CXFA_Node::Script_Form_Checksum(CFXJSE_Value* pValue,
                                     bool bSetting,
                                     XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Form_Checksum(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Packet_GetAttribute(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Packet_GetAttribute(pArguments);
}

void CXFA_Node::Script_Packet_SetAttribute(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Packet_SetAttribute(pArguments);
}

void CXFA_Node::Script_Packet_RemoveAttribute(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Packet_RemoveAttribute(pArguments);
}

void CXFA_Node::Script_Packet_Content(CFXJSE_Value* pValue,
                                      bool bSetting,
                                      XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Packet_Content(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Source_Next(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_Next(pArguments);
}

void CXFA_Node::Script_Source_CancelBatch(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_CancelBatch(pArguments);
}

void CXFA_Node::Script_Source_First(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_First(pArguments);
}

void CXFA_Node::Script_Source_UpdateBatch(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_UpdateBatch(pArguments);
}

void CXFA_Node::Script_Source_Previous(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_Previous(pArguments);
}

void CXFA_Node::Script_Source_IsBOF(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_IsBOF(pArguments);
}

void CXFA_Node::Script_Source_IsEOF(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_IsEOF(pArguments);
}

void CXFA_Node::Script_Source_Cancel(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_Cancel(pArguments);
}

void CXFA_Node::Script_Source_Update(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_Update(pArguments);
}

void CXFA_Node::Script_Source_Open(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_Open(pArguments);
}

void CXFA_Node::Script_Source_Delete(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_Delete(pArguments);
}

void CXFA_Node::Script_Source_AddNew(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_AddNew(pArguments);
}

void CXFA_Node::Script_Source_Requery(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_Requery(pArguments);
}

void CXFA_Node::Script_Source_Resync(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_Resync(pArguments);
}

void CXFA_Node::Script_Source_Close(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_Close(pArguments);
}

void CXFA_Node::Script_Source_Last(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_Last(pArguments);
}

void CXFA_Node::Script_Source_HasDataChanged(CFXJSE_Arguments* pArguments) {
  JSNode()->Script_Source_HasDataChanged(pArguments);
}

void CXFA_Node::Script_Source_Db(CFXJSE_Value* pValue,
                                 bool bSetting,
                                 XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Source_Db(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Xfa_This(CFXJSE_Value* pValue,
                                bool bSetting,
                                XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Xfa_This(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Handler_Version(CFXJSE_Value* pValue,
                                       bool bSetting,
                                       XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Handler_Version(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_SubmitFormat_Mode(CFXJSE_Value* pValue,
                                         bool bSetting,
                                         XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_SubmitFormat_Mode(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Extras_Type(CFXJSE_Value* pValue,
                                   bool bSetting,
                                   XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Extras_Type(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Script_Stateless(CFXJSE_Value* pValue,
                                        bool bSetting,
                                        XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Script_Stateless(pValue, bSetting, eAttribute);
}

void CXFA_Node::Script_Encrypt_Format(CFXJSE_Value* pValue,
                                      bool bSetting,
                                      XFA_ATTRIBUTE eAttribute) {
  JSNode()->Script_Encrypt_Format(pValue, bSetting, eAttribute);
}

CXFA_Node* CXFA_Node::GetModelNode() {
  switch (GetPacketID()) {
    case XFA_XDPPACKET_XDP:
      return m_pDocument->GetRoot();
    case XFA_XDPPACKET_Config:
      return ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_Config));
    case XFA_XDPPACKET_Template:
      return ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_Template));
    case XFA_XDPPACKET_Form:
      return ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_Form));
    case XFA_XDPPACKET_Datasets:
      return ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_Datasets));
    case XFA_XDPPACKET_LocaleSet:
      return ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_LocaleSet));
    case XFA_XDPPACKET_ConnectionSet:
      return ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_ConnectionSet));
    case XFA_XDPPACKET_SourceSet:
      return ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_SourceSet));
    case XFA_XDPPACKET_Xdc:
      return ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_Xdc));
    default:
      return this;
  }
}

int32_t CXFA_Node::CountChildren(XFA_Element eType, bool bOnlyChild) {
  CXFA_Node* pNode = m_pChild;
  int32_t iCount = 0;
  for (; pNode; pNode = pNode->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    if (pNode->GetElementType() == eType || eType == XFA_Element::Unknown) {
      if (bOnlyChild) {
        const XFA_PROPERTY* pProperty = XFA_GetPropertyOfElement(
            GetElementType(), pNode->GetElementType(), XFA_XDPPACKET_UNKNOWN);
        if (pProperty) {
          continue;
        }
      }
      iCount++;
    }
  }
  return iCount;
}

CXFA_Node* CXFA_Node::GetChild(int32_t index,
                               XFA_Element eType,
                               bool bOnlyChild) {
  ASSERT(index > -1);
  CXFA_Node* pNode = m_pChild;
  int32_t iCount = 0;
  for (; pNode; pNode = pNode->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    if (pNode->GetElementType() == eType || eType == XFA_Element::Unknown) {
      if (bOnlyChild) {
        const XFA_PROPERTY* pProperty = XFA_GetPropertyOfElement(
            GetElementType(), pNode->GetElementType(), XFA_XDPPACKET_UNKNOWN);
        if (pProperty) {
          continue;
        }
      }
      iCount++;
      if (iCount > index) {
        return pNode;
      }
    }
  }
  return nullptr;
}

int32_t CXFA_Node::InsertChild(int32_t index, CXFA_Node* pNode) {
  ASSERT(!pNode->m_pNext);
  pNode->m_pParent = this;
  bool ret = m_pDocument->RemovePurgeNode(pNode);
  ASSERT(ret);
  (void)ret;  // Avoid unused variable warning.

  if (!m_pChild || index == 0) {
    if (index > 0) {
      return -1;
    }
    pNode->m_pNext = m_pChild;
    m_pChild = pNode;
    index = 0;
  } else if (index < 0) {
    m_pLastChild->m_pNext = pNode;
  } else {
    CXFA_Node* pPrev = m_pChild;
    int32_t iCount = 0;
    while (++iCount != index && pPrev->m_pNext) {
      pPrev = pPrev->m_pNext;
    }
    if (index > 0 && index != iCount) {
      return -1;
    }
    pNode->m_pNext = pPrev->m_pNext;
    pPrev->m_pNext = pNode;
    index = iCount;
  }
  if (!pNode->m_pNext) {
    m_pLastChild = pNode;
  }
  ASSERT(m_pLastChild);
  ASSERT(!m_pLastChild->m_pNext);
  pNode->ClearFlag(XFA_NodeFlag_HasRemovedChildren);
  CXFA_FFNotify* pNotify = m_pDocument->GetNotify();
  if (pNotify)
    pNotify->OnChildAdded(this);

  if (IsNeedSavingXMLNode() && pNode->m_pXMLNode) {
    ASSERT(!pNode->m_pXMLNode->GetNodeItem(CFX_XMLNode::Parent));
    m_pXMLNode->InsertChildNode(pNode->m_pXMLNode, index);
    pNode->ClearFlag(XFA_NodeFlag_OwnXMLNode);
  }
  return index;
}

bool CXFA_Node::InsertChild(CXFA_Node* pNode, CXFA_Node* pBeforeNode) {
  if (!pNode || pNode->m_pParent ||
      (pBeforeNode && pBeforeNode->m_pParent != this)) {
    NOTREACHED();
    return false;
  }
  bool ret = m_pDocument->RemovePurgeNode(pNode);
  ASSERT(ret);
  (void)ret;  // Avoid unused variable warning.

  int32_t nIndex = -1;
  pNode->m_pParent = this;
  if (!m_pChild || pBeforeNode == m_pChild) {
    pNode->m_pNext = m_pChild;
    m_pChild = pNode;
    nIndex = 0;
  } else if (!pBeforeNode) {
    pNode->m_pNext = m_pLastChild->m_pNext;
    m_pLastChild->m_pNext = pNode;
  } else {
    nIndex = 1;
    CXFA_Node* pPrev = m_pChild;
    while (pPrev->m_pNext != pBeforeNode) {
      pPrev = pPrev->m_pNext;
      nIndex++;
    }
    pNode->m_pNext = pPrev->m_pNext;
    pPrev->m_pNext = pNode;
  }
  if (!pNode->m_pNext) {
    m_pLastChild = pNode;
  }
  ASSERT(m_pLastChild);
  ASSERT(!m_pLastChild->m_pNext);
  pNode->ClearFlag(XFA_NodeFlag_HasRemovedChildren);
  CXFA_FFNotify* pNotify = m_pDocument->GetNotify();
  if (pNotify)
    pNotify->OnChildAdded(this);

  if (IsNeedSavingXMLNode() && pNode->m_pXMLNode) {
    ASSERT(!pNode->m_pXMLNode->GetNodeItem(CFX_XMLNode::Parent));
    m_pXMLNode->InsertChildNode(pNode->m_pXMLNode, nIndex);
    pNode->ClearFlag(XFA_NodeFlag_OwnXMLNode);
  }
  return true;
}

CXFA_Node* CXFA_Node::Deprecated_GetPrevSibling() {
  if (!m_pParent) {
    return nullptr;
  }
  for (CXFA_Node* pSibling = m_pParent->m_pChild; pSibling;
       pSibling = pSibling->m_pNext) {
    if (pSibling->m_pNext == this) {
      return pSibling;
    }
  }
  return nullptr;
}

bool CXFA_Node::RemoveChild(CXFA_Node* pNode, bool bNotify) {
  if (!pNode || pNode->m_pParent != this) {
    NOTREACHED();
    return false;
  }
  if (m_pChild == pNode) {
    m_pChild = pNode->m_pNext;
    if (m_pLastChild == pNode) {
      m_pLastChild = pNode->m_pNext;
    }
    pNode->m_pNext = nullptr;
    pNode->m_pParent = nullptr;
  } else {
    CXFA_Node* pPrev = pNode->Deprecated_GetPrevSibling();
    pPrev->m_pNext = pNode->m_pNext;
    if (m_pLastChild == pNode) {
      m_pLastChild = pNode->m_pNext ? pNode->m_pNext : pPrev;
    }
    pNode->m_pNext = nullptr;
    pNode->m_pParent = nullptr;
  }
  ASSERT(!m_pLastChild || !m_pLastChild->m_pNext);
  OnRemoved(bNotify);
  pNode->SetFlag(XFA_NodeFlag_HasRemovedChildren, true);
  m_pDocument->AddPurgeNode(pNode);
  if (IsNeedSavingXMLNode() && pNode->m_pXMLNode) {
    if (pNode->IsAttributeInXML()) {
      ASSERT(pNode->m_pXMLNode == m_pXMLNode &&
             m_pXMLNode->GetType() == FX_XMLNODE_Element);
      if (pNode->m_pXMLNode->GetType() == FX_XMLNODE_Element) {
        CFX_XMLElement* pXMLElement =
            static_cast<CFX_XMLElement*>(pNode->m_pXMLNode);
        WideStringView wsAttributeName =
            pNode->JSNode()->GetCData(XFA_ATTRIBUTE_QualifiedName);
        // TODO(tsepez): check usage of c_str() below.
        pXMLElement->RemoveAttribute(wsAttributeName.unterminated_c_str());
      }
      WideString wsName;
      pNode->JSNode()->GetAttribute(XFA_ATTRIBUTE_Name, wsName, false);
      CFX_XMLElement* pNewXMLElement = new CFX_XMLElement(wsName);
      WideStringView wsValue = JSNode()->GetCData(XFA_ATTRIBUTE_Value);
      if (!wsValue.IsEmpty()) {
        pNewXMLElement->SetTextData(WideString(wsValue));
      }
      pNode->m_pXMLNode = pNewXMLElement;
      pNode->JSNode()->SetEnum(XFA_ATTRIBUTE_Contains,
                               XFA_ATTRIBUTEENUM_Unknown);
    } else {
      m_pXMLNode->RemoveChildNode(pNode->m_pXMLNode);
    }
    pNode->SetFlag(XFA_NodeFlag_OwnXMLNode, false);
  }
  return true;
}

CXFA_Node* CXFA_Node::GetFirstChildByName(const WideStringView& wsName) const {
  return GetFirstChildByName(FX_HashCode_GetW(wsName, false));
}

CXFA_Node* CXFA_Node::GetFirstChildByName(uint32_t dwNameHash) const {
  for (CXFA_Node* pNode = GetNodeItem(XFA_NODEITEM_FirstChild); pNode;
       pNode = pNode->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    if (pNode->GetNameHash() == dwNameHash) {
      return pNode;
    }
  }
  return nullptr;
}

CXFA_Node* CXFA_Node::GetFirstChildByClass(XFA_Element eType) const {
  for (CXFA_Node* pNode = GetNodeItem(XFA_NODEITEM_FirstChild); pNode;
       pNode = pNode->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    if (pNode->GetElementType() == eType) {
      return pNode;
    }
  }
  return nullptr;
}

CXFA_Node* CXFA_Node::GetNextSameNameSibling(uint32_t dwNameHash) const {
  for (CXFA_Node* pNode = GetNodeItem(XFA_NODEITEM_NextSibling); pNode;
       pNode = pNode->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    if (pNode->GetNameHash() == dwNameHash) {
      return pNode;
    }
  }
  return nullptr;
}

CXFA_Node* CXFA_Node::GetNextSameNameSibling(
    const WideStringView& wsNodeName) const {
  return GetNextSameNameSibling(FX_HashCode_GetW(wsNodeName, false));
}

CXFA_Node* CXFA_Node::GetNextSameClassSibling(XFA_Element eType) const {
  for (CXFA_Node* pNode = GetNodeItem(XFA_NODEITEM_NextSibling); pNode;
       pNode = pNode->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    if (pNode->GetElementType() == eType) {
      return pNode;
    }
  }
  return nullptr;
}

int32_t CXFA_Node::GetNodeSameNameIndex() const {
  CFXJSE_Engine* pScriptContext = m_pDocument->GetScriptContext();
  if (!pScriptContext) {
    return -1;
  }
  return pScriptContext->GetIndexByName(const_cast<CXFA_Node*>(this));
}

int32_t CXFA_Node::GetNodeSameClassIndex() const {
  CFXJSE_Engine* pScriptContext = m_pDocument->GetScriptContext();
  if (!pScriptContext) {
    return -1;
  }
  return pScriptContext->GetIndexByClassName(const_cast<CXFA_Node*>(this));
}

void CXFA_Node::GetSOMExpression(WideString& wsSOMExpression) {
  CFXJSE_Engine* pScriptContext = m_pDocument->GetScriptContext();
  if (!pScriptContext) {
    return;
  }
  pScriptContext->GetSomExpression(this, wsSOMExpression);
}

CXFA_Node* CXFA_Node::GetInstanceMgrOfSubform() {
  CXFA_Node* pInstanceMgr = nullptr;
  if (m_ePacket == XFA_XDPPACKET_Form) {
    CXFA_Node* pParentNode = GetNodeItem(XFA_NODEITEM_Parent);
    if (!pParentNode || pParentNode->GetElementType() == XFA_Element::Area) {
      return pInstanceMgr;
    }
    for (CXFA_Node* pNode = GetNodeItem(XFA_NODEITEM_PrevSibling); pNode;
         pNode = pNode->GetNodeItem(XFA_NODEITEM_PrevSibling)) {
      XFA_Element eType = pNode->GetElementType();
      if ((eType == XFA_Element::Subform || eType == XFA_Element::SubformSet) &&
          pNode->m_dwNameHash != m_dwNameHash) {
        break;
      }
      if (eType == XFA_Element::InstanceManager) {
        WideStringView wsName = JSNode()->GetCData(XFA_ATTRIBUTE_Name);
        WideStringView wsInstName =
            pNode->JSNode()->GetCData(XFA_ATTRIBUTE_Name);
        if (wsInstName.GetLength() > 0 && wsInstName[0] == '_' &&
            wsInstName.Right(wsInstName.GetLength() - 1) == wsName) {
          pInstanceMgr = pNode;
        }
        break;
      }
    }
  }
  return pInstanceMgr;
}

CXFA_Node* CXFA_Node::GetOccurNode() {
  return GetFirstChildByClass(XFA_Element::Occur);
}

bool CXFA_Node::HasFlag(XFA_NodeFlag dwFlag) const {
  if (m_uNodeFlags & dwFlag)
    return true;
  if (dwFlag == XFA_NodeFlag_HasRemovedChildren)
    return m_pParent && m_pParent->HasFlag(dwFlag);
  return false;
}

void CXFA_Node::SetFlag(uint32_t dwFlag, bool bNotify) {
  if (dwFlag == XFA_NodeFlag_Initialized && bNotify && !IsInitialized()) {
    CXFA_FFNotify* pNotify = m_pDocument->GetNotify();
    if (pNotify) {
      pNotify->OnNodeReady(this);
    }
  }
  m_uNodeFlags |= dwFlag;
}

void CXFA_Node::ClearFlag(uint32_t dwFlag) {
  m_uNodeFlags &= ~dwFlag;
}

bool CXFA_Node::IsAttributeInXML() {
  return JSNode()->GetEnum(XFA_ATTRIBUTE_Contains) ==
         XFA_ATTRIBUTEENUM_MetaData;
}

void CXFA_Node::OnRemoved(bool bNotify) {
  if (!bNotify)
    return;

  CXFA_FFNotify* pNotify = m_pDocument->GetNotify();
  if (pNotify)
    pNotify->OnChildRemoved();
}

void CXFA_Node::OnChanging(XFA_ATTRIBUTE eAttr, bool bNotify) {
  if (bNotify && IsInitialized()) {
    CXFA_FFNotify* pNotify = m_pDocument->GetNotify();
    if (pNotify) {
      pNotify->OnValueChanging(this, eAttr);
    }
  }
}

void CXFA_Node::OnChanged(XFA_ATTRIBUTE eAttr,
                          bool bNotify,
                          bool bScriptModify) {
  if (bNotify && IsInitialized()) {
    Script_Attribute_SendAttributeChangeMessage(eAttr, bScriptModify);
  }
}

void CXFA_Node::UpdateNameHash() {
  const XFA_NOTSUREATTRIBUTE* pNotsure =
      XFA_GetNotsureAttribute(GetElementType(), XFA_ATTRIBUTE_Name);
  WideStringView wsName;
  if (!pNotsure || pNotsure->eType == XFA_ATTRIBUTETYPE_Cdata) {
    wsName = JSNode()->GetCData(XFA_ATTRIBUTE_Name);
    m_dwNameHash = FX_HashCode_GetW(wsName, false);
  } else if (pNotsure->eType == XFA_ATTRIBUTETYPE_Enum) {
    wsName = GetAttributeEnumByID(JSNode()->GetEnum(XFA_ATTRIBUTE_Name))->pName;
    m_dwNameHash = FX_HashCode_GetW(wsName, false);
  }
}

CFX_XMLNode* CXFA_Node::CreateXMLMappingNode() {
  if (!m_pXMLNode) {
    WideString wsTag(JSNode()->GetCData(XFA_ATTRIBUTE_Name));
    m_pXMLNode = new CFX_XMLElement(wsTag);
    SetFlag(XFA_NodeFlag_OwnXMLNode, false);
  }
  return m_pXMLNode;
}

bool CXFA_Node::IsNeedSavingXMLNode() {
  return m_pXMLNode && (GetPacketID() == XFA_XDPPACKET_Datasets ||
                        GetElementType() == XFA_Element::Xfa);
}

CXFA_Node* CXFA_Node::GetItem(CXFA_Node* pInstMgrNode, int32_t iIndex) {
  ASSERT(pInstMgrNode);
  int32_t iCount = 0;
  uint32_t dwNameHash = 0;
  for (CXFA_Node* pNode = pInstMgrNode->GetNodeItem(XFA_NODEITEM_NextSibling);
       pNode; pNode = pNode->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    XFA_Element eCurType = pNode->GetElementType();
    if (eCurType == XFA_Element::InstanceManager)
      break;
    if ((eCurType != XFA_Element::Subform) &&
        (eCurType != XFA_Element::SubformSet)) {
      continue;
    }
    if (iCount == 0) {
      WideStringView wsName = pNode->JSNode()->GetCData(XFA_ATTRIBUTE_Name);
      WideStringView wsInstName =
          pInstMgrNode->JSNode()->GetCData(XFA_ATTRIBUTE_Name);
      if (wsInstName.GetLength() < 1 || wsInstName[0] != '_' ||
          wsInstName.Right(wsInstName.GetLength() - 1) != wsName) {
        return nullptr;
      }
      dwNameHash = pNode->GetNameHash();
    }
    if (dwNameHash != pNode->GetNameHash())
      break;

    iCount++;
    if (iCount > iIndex)
      return pNode;
  }
  return nullptr;
}

int32_t CXFA_Node::GetCount(CXFA_Node* pInstMgrNode) {
  ASSERT(pInstMgrNode);
  int32_t iCount = 0;
  uint32_t dwNameHash = 0;
  for (CXFA_Node* pNode = pInstMgrNode->GetNodeItem(XFA_NODEITEM_NextSibling);
       pNode; pNode = pNode->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    XFA_Element eCurType = pNode->GetElementType();
    if (eCurType == XFA_Element::InstanceManager)
      break;
    if ((eCurType != XFA_Element::Subform) &&
        (eCurType != XFA_Element::SubformSet)) {
      continue;
    }
    if (iCount == 0) {
      WideStringView wsName = pNode->JSNode()->GetCData(XFA_ATTRIBUTE_Name);
      WideStringView wsInstName =
          pInstMgrNode->JSNode()->GetCData(XFA_ATTRIBUTE_Name);
      if (wsInstName.GetLength() < 1 || wsInstName[0] != '_' ||
          wsInstName.Right(wsInstName.GetLength() - 1) != wsName) {
        return iCount;
      }
      dwNameHash = pNode->GetNameHash();
    }
    if (dwNameHash != pNode->GetNameHash())
      break;

    iCount++;
  }
  return iCount;
}

void CXFA_Node::InsertItem(CXFA_Node* pInstMgrNode,
                           CXFA_Node* pNewInstance,
                           int32_t iPos,
                           int32_t iCount,
                           bool bMoveDataBindingNodes) {
  if (iCount < 0)
    iCount = GetCount(pInstMgrNode);
  if (iPos < 0)
    iPos = iCount;
  if (iPos == iCount) {
    CXFA_Node* pNextSibling =
        iCount > 0 ? pInstMgrNode->GetItem(pInstMgrNode, iCount - 1)
                         ->GetNodeItem(XFA_NODEITEM_NextSibling)
                   : pInstMgrNode->GetNodeItem(XFA_NODEITEM_NextSibling);
    pInstMgrNode->GetNodeItem(XFA_NODEITEM_Parent)
        ->InsertChild(pNewInstance, pNextSibling);
    if (bMoveDataBindingNodes) {
      std::set<CXFA_Node*> sNew;
      std::set<CXFA_Node*> sAfter;
      CXFA_NodeIteratorTemplate<CXFA_Node,
                                CXFA_TraverseStrategy_XFAContainerNode>
          sIteratorNew(pNewInstance);
      for (CXFA_Node* pNode = sIteratorNew.GetCurrent(); pNode;
           pNode = sIteratorNew.MoveToNext()) {
        CXFA_Node* pDataNode = pNode->GetBindData();
        if (!pDataNode)
          continue;

        sNew.insert(pDataNode);
      }
      CXFA_NodeIteratorTemplate<CXFA_Node,
                                CXFA_TraverseStrategy_XFAContainerNode>
          sIteratorAfter(pNextSibling);
      for (CXFA_Node* pNode = sIteratorAfter.GetCurrent(); pNode;
           pNode = sIteratorAfter.MoveToNext()) {
        CXFA_Node* pDataNode = pNode->GetBindData();
        if (!pDataNode)
          continue;

        sAfter.insert(pDataNode);
      }
      ReorderDataNodes(sNew, sAfter, false);
    }
  } else {
    CXFA_Node* pBeforeInstance = GetItem(pInstMgrNode, iPos);
    pInstMgrNode->GetNodeItem(XFA_NODEITEM_Parent)
        ->InsertChild(pNewInstance, pBeforeInstance);
    if (bMoveDataBindingNodes) {
      std::set<CXFA_Node*> sNew;
      std::set<CXFA_Node*> sBefore;
      CXFA_NodeIteratorTemplate<CXFA_Node,
                                CXFA_TraverseStrategy_XFAContainerNode>
          sIteratorNew(pNewInstance);
      for (CXFA_Node* pNode = sIteratorNew.GetCurrent(); pNode;
           pNode = sIteratorNew.MoveToNext()) {
        CXFA_Node* pDataNode = pNode->GetBindData();
        if (!pDataNode)
          continue;

        sNew.insert(pDataNode);
      }
      CXFA_NodeIteratorTemplate<CXFA_Node,
                                CXFA_TraverseStrategy_XFAContainerNode>
          sIteratorBefore(pBeforeInstance);
      for (CXFA_Node* pNode = sIteratorBefore.GetCurrent(); pNode;
           pNode = sIteratorBefore.MoveToNext()) {
        CXFA_Node* pDataNode = pNode->GetBindData();
        if (!pDataNode)
          continue;

        sBefore.insert(pDataNode);
      }
      ReorderDataNodes(sNew, sBefore, true);
    }
  }
}

void CXFA_Node::RemoveItem(CXFA_Node* pInstMgrNode,
                           CXFA_Node* pRemoveInstance,
                           bool bRemoveDataBinding) {
  pInstMgrNode->GetNodeItem(XFA_NODEITEM_Parent)->RemoveChild(pRemoveInstance);
  if (!bRemoveDataBinding)
    return;

  CXFA_NodeIteratorTemplate<CXFA_Node, CXFA_TraverseStrategy_XFAContainerNode>
      sIterator(pRemoveInstance);
  for (CXFA_Node* pFormNode = sIterator.GetCurrent(); pFormNode;
       pFormNode = sIterator.MoveToNext()) {
    CXFA_Node* pDataNode = pFormNode->GetBindData();
    if (!pDataNode)
      continue;

    if (pDataNode->RemoveBindItem(pFormNode) == 0) {
      if (CXFA_Node* pDataParent =
              pDataNode->GetNodeItem(XFA_NODEITEM_Parent)) {
        pDataParent->RemoveChild(pDataNode);
      }
    }
    pFormNode->JSNode()->SetObject(XFA_ATTRIBUTE_BindingNode, nullptr);
  }
}

CXFA_Node* CXFA_Node::CreateInstance(CXFA_Node* pInstMgrNode, bool bDataMerge) {
  CXFA_Document* pDocument = pInstMgrNode->GetDocument();
  CXFA_Node* pTemplateNode = pInstMgrNode->GetTemplateNode();
  CXFA_Node* pFormParent = pInstMgrNode->GetNodeItem(XFA_NODEITEM_Parent);
  CXFA_Node* pDataScope = nullptr;
  for (CXFA_Node* pRootBoundNode = pFormParent;
       pRootBoundNode && pRootBoundNode->IsContainerNode();
       pRootBoundNode = pRootBoundNode->GetNodeItem(XFA_NODEITEM_Parent)) {
    pDataScope = pRootBoundNode->GetBindData();
    if (pDataScope)
      break;
  }
  if (!pDataScope) {
    pDataScope = ToNode(pDocument->GetXFAObject(XFA_HASHCODE_Record));
    ASSERT(pDataScope);
  }
  CXFA_Node* pInstance = pDocument->DataMerge_CopyContainer(
      pTemplateNode, pFormParent, pDataScope, true, bDataMerge, true);
  if (pInstance) {
    pDocument->DataMerge_UpdateBindingRelations(pInstance);
    pFormParent->RemoveChild(pInstance);
  }
  return pInstance;
}


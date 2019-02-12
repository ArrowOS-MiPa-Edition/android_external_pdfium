// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdftext/cpdf_textpagefind.h"

#include <cwchar>
#include <cwctype>
#include <vector>

#include "core/fpdftext/cpdf_textpage.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "third_party/base/stl_util.h"

namespace {

bool IsIgnoreSpaceCharacter(wchar_t curChar) {
  if (curChar < 255 || (curChar >= 0x0600 && curChar <= 0x06FF) ||
      (curChar >= 0xFE70 && curChar <= 0xFEFF) ||
      (curChar >= 0xFB50 && curChar <= 0xFDFF) ||
      (curChar >= 0x0400 && curChar <= 0x04FF) ||
      (curChar >= 0x0500 && curChar <= 0x052F) ||
      (curChar >= 0xA640 && curChar <= 0xA69F) ||
      (curChar >= 0x2DE0 && curChar <= 0x2DFF) || curChar == 8467 ||
      (curChar >= 0x2000 && curChar <= 0x206F)) {
    return false;
  }
  return true;
}

}  // namespace

CPDF_TextPageFind::CPDF_TextPageFind(const CPDF_TextPage* pTextPage)
    : m_pTextPage(pTextPage) {
  ASSERT(m_pTextPage);

  m_strText = m_pTextPage->GetAllPageText();
}

CPDF_TextPageFind::~CPDF_TextPageFind() {}

int CPDF_TextPageFind::GetCharIndex(int index) const {
  return m_pTextPage->CharIndexFromTextIndex(index);
}

bool CPDF_TextPageFind::FindFirst(const WideString& findwhat,
                                  const Options& options,
                                  Optional<size_t> startPos) {
  if (m_strText.IsEmpty() || m_bMatchCase != options.bMatchCase)
    m_strText = m_pTextPage->GetAllPageText();
  WideString findwhatStr = findwhat;
  m_findWhat = findwhatStr;
  m_options = options;
  m_bMatchCase = options.bMatchCase;
  if (m_strText.IsEmpty())
    return true;

  size_t len = findwhatStr.GetLength();
  if (!m_bMatchCase) {
    findwhatStr.MakeLower();
    m_strText.MakeLower();
  }
  m_bMatchWholeWord = options.bMatchWholeWord;
  m_findNextStart = startPos;
  if (!startPos.has_value()) {
    if (!m_strText.IsEmpty())
      m_findPreStart = m_strText.GetLength() - 1;
  } else {
    m_findPreStart = startPos;
  }

  m_csFindWhatArray.clear();
  size_t i = 0;
  for (i = 0; i < len; ++i)
    if (findwhatStr[i] != ' ')
      break;
  if (i < len)
    ExtractFindWhat(findwhatStr);
  else
    m_csFindWhatArray.push_back(findwhatStr);
  if (m_csFindWhatArray.empty())
    return false;

  m_resStart = 0;
  m_resEnd = -1;
  return true;
}

bool CPDF_TextPageFind::FindNext() {
  if (!m_findNextStart.has_value())
    return false;
  if (m_strText.IsEmpty())
    return false;

  size_t strLen = m_strText.GetLength();
  if (m_findNextStart.value() > strLen - 1)
    return false;

  int nCount = pdfium::CollectionSize<int>(m_csFindWhatArray);
  Optional<size_t> nResultPos = 0;
  size_t nStartPos = m_findNextStart.value();
  bool bSpaceStart = false;
  for (int iWord = 0; iWord < nCount; iWord++) {
    WideString csWord = m_csFindWhatArray[iWord];
    if (csWord.IsEmpty()) {
      if (iWord == nCount - 1) {
        wchar_t strInsert = m_strText[nStartPos];
        if (strInsert == TEXT_LINEFEED_CHAR || strInsert == TEXT_SPACE_CHAR ||
            strInsert == TEXT_RETURN_CHAR || strInsert == 160) {
          nResultPos = nStartPos + 1;
          break;
        }
        iWord = -1;
      } else if (iWord == 0) {
        bSpaceStart = true;
      }
      continue;
    }
    nResultPos = m_strText.Find(csWord.AsStringView(), nStartPos);
    if (!nResultPos.has_value())
      return false;

    size_t endIndex = nResultPos.value() + csWord.GetLength() - 1;
    if (iWord == 0)
      m_resStart = nResultPos.value();
    bool bMatch = true;
    if (iWord != 0 && !bSpaceStart) {
      size_t PreResEndPos = nStartPos;
      int curChar = csWord[0];
      WideString lastWord = m_csFindWhatArray[iWord - 1];
      int lastChar = lastWord[lastWord.GetLength() - 1];
      if (nStartPos == nResultPos.value() &&
          !(IsIgnoreSpaceCharacter(lastChar) ||
            IsIgnoreSpaceCharacter(curChar))) {
        bMatch = false;
      }
      for (size_t d = PreResEndPos; d < nResultPos.value(); d++) {
        wchar_t strInsert = m_strText[d];
        if (strInsert != TEXT_LINEFEED_CHAR && strInsert != TEXT_SPACE_CHAR &&
            strInsert != TEXT_RETURN_CHAR && strInsert != 160) {
          bMatch = false;
          break;
        }
      }
    } else if (bSpaceStart) {
      if (nResultPos.value() > 0) {
        wchar_t strInsert = m_strText[nResultPos.value() - 1];
        if (strInsert != TEXT_LINEFEED_CHAR && strInsert != TEXT_SPACE_CHAR &&
            strInsert != TEXT_RETURN_CHAR && strInsert != 160) {
          bMatch = false;
          m_resStart = nResultPos.value();
        } else {
          m_resStart = nResultPos.value() - 1;
        }
      }
    }
    if (m_bMatchWholeWord && bMatch) {
      bMatch = IsMatchWholeWord(m_strText, nResultPos.value(), endIndex);
    }
    nStartPos = endIndex + 1;
    if (!bMatch) {
      iWord = -1;
      if (bSpaceStart)
        nStartPos = m_resStart + m_csFindWhatArray[1].GetLength();
      else
        nStartPos = m_resStart + m_csFindWhatArray[0].GetLength();
    }
  }
  m_resEnd = nResultPos.value() + m_csFindWhatArray.back().GetLength() - 1;
  if (m_options.bConsecutive) {
    m_findNextStart = m_resStart + 1;
    m_findPreStart = m_resEnd - 1;
  } else {
    m_findNextStart = m_resEnd + 1;
    m_findPreStart = m_resStart - 1;
  }
  return true;
}

bool CPDF_TextPageFind::FindPrev() {
  if (m_strText.IsEmpty() || !m_findPreStart.has_value())
    return false;

  CPDF_TextPageFind find_engine(m_pTextPage.Get());
  if (!find_engine.FindFirst(m_findWhat, m_options, 0))
    return false;

  int order = -1;
  int matches = 0;
  while (find_engine.FindNext()) {
    int cur_order = find_engine.GetCurOrder();
    int cur_match = find_engine.GetMatchedCount();
    int temp = cur_order + cur_match;
    if (temp < 0 || static_cast<size_t>(temp) > m_findPreStart.value() + 1)
      break;

    order = cur_order;
    matches = cur_match;
  }
  if (order == -1)
    return false;

  m_resStart = m_pTextPage->TextIndexFromCharIndex(order);
  m_resEnd = m_pTextPage->TextIndexFromCharIndex(order + matches - 1);
  if (m_options.bConsecutive) {
    m_findNextStart = m_resStart + 1;
    m_findPreStart = m_resEnd - 1;
  } else {
    m_findNextStart = m_resEnd + 1;
    m_findPreStart = m_resStart - 1;
  }
  return true;
}

void CPDF_TextPageFind::ExtractFindWhat(const WideString& findwhat) {
  if (findwhat.IsEmpty())
    return;
  int index = 0;
  while (1) {
    Optional<WideString> word =
        ExtractSubString(findwhat.c_str(), index, TEXT_SPACE_CHAR);
    if (!word)
      break;

    if (word->IsEmpty()) {
      m_csFindWhatArray.push_back(L"");
      index++;
      continue;
    }

    size_t pos = 0;
    while (pos < word->GetLength()) {
      WideString curStr = word->Mid(pos, 1);
      wchar_t curChar = (*word)[pos];
      if (IsIgnoreSpaceCharacter(curChar)) {
        if (pos > 0 && curChar == 0x2019) {
          pos++;
          continue;
        }
        if (pos > 0)
          m_csFindWhatArray.push_back(word->Left(pos));
        m_csFindWhatArray.push_back(curStr);
        if (pos == word->GetLength() - 1) {
          word->clear();
          break;
        }
        word.emplace(word->Right(word->GetLength() - pos - 1));
        pos = 0;
        continue;
      }
      pos++;
    }

    if (!word->IsEmpty())
      m_csFindWhatArray.push_back(word.value());
    index++;
  }
}

bool CPDF_TextPageFind::IsMatchWholeWord(const WideString& csPageText,
                                         size_t startPos,
                                         size_t endPos) {
  if (startPos > endPos)
    return false;
  wchar_t char_left = 0;
  wchar_t char_right = 0;
  size_t char_count = endPos - startPos + 1;
  if (char_count == 0)
    return false;
  if (char_count == 1 && csPageText[startPos] > 255)
    return true;
  if (startPos >= 1)
    char_left = csPageText[startPos - 1];
  if (startPos + char_count < csPageText.GetLength())
    char_right = csPageText[startPos + char_count];
  if ((char_left > 'A' && char_left < 'a') ||
      (char_left > 'a' && char_left < 'z') ||
      (char_left > 0xfb00 && char_left < 0xfb06) ||
      FXSYS_IsDecimalDigit(char_left) ||
      (char_right > 'A' && char_right < 'a') ||
      (char_right > 'a' && char_right < 'z') ||
      (char_right > 0xfb00 && char_right < 0xfb06) ||
      FXSYS_IsDecimalDigit(char_right)) {
    return false;
  }
  if (!(('A' > char_left || char_left > 'Z') &&
        ('a' > char_left || char_left > 'z') &&
        ('A' > char_right || char_right > 'Z') &&
        ('a' > char_right || char_right > 'z'))) {
    return false;
  }
  if (char_count > 0) {
    if (FXSYS_IsDecimalDigit(char_left) &&
        FXSYS_IsDecimalDigit(csPageText[startPos])) {
      return false;
    }
    if (FXSYS_IsDecimalDigit(char_right) &&
        FXSYS_IsDecimalDigit(csPageText[endPos])) {
      return false;
    }
  }
  return true;
}

Optional<WideString> CPDF_TextPageFind::ExtractSubString(
    const wchar_t* lpszFullString,
    int iSubString,
    wchar_t chSep) {
  if (!lpszFullString)
    return {};

  while (iSubString--) {
    lpszFullString = std::wcschr(lpszFullString, chSep);
    if (!lpszFullString)
      return {};

    lpszFullString++;
    while (*lpszFullString == chSep)
      lpszFullString++;
  }

  const wchar_t* lpchEnd = std::wcschr(lpszFullString, chSep);
  int nLen = lpchEnd ? static_cast<int>(lpchEnd - lpszFullString)
                     : static_cast<int>(wcslen(lpszFullString));
  if (nLen < 0)
    return {};

  return {WideString(lpszFullString, static_cast<size_t>(nLen))};
}

int CPDF_TextPageFind::GetCurOrder() const {
  return GetCharIndex(m_resStart);
}

int CPDF_TextPageFind::GetMatchedCount() const {
  int resStart = GetCharIndex(m_resStart);
  int resEnd = GetCharIndex(m_resEnd);
  return resEnd - resStart + 1;
}

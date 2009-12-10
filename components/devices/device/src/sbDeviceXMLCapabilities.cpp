/* vim: set sw=2 :miv */
/*
 *=BEGIN SONGBIRD GPL
 *
 * This file is part of the Songbird web player.
 *
 * Copyright(c) 2005-2009 POTI, Inc.
 * http://www.songbirdnest.com
 *
 * This file may be licensed under the terms of of the
 * GNU General Public License Version 2 (the ``GPL'').
 *
 * Software distributed under the License is distributed
 * on an ``AS IS'' basis, WITHOUT WARRANTY OF ANY KIND, either
 * express or implied. See the GPL for the specific language
 * governing rights and limitations.
 *
 * You should have received a copy of the GPL along with this
 * program. If not, go to http://www.gnu.org/licenses/gpl.html
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *=END SONGBIRD GPL
 */

#include "sbDeviceXMLCapabilities.h"

// Songbird includes
#include <sbIDeviceCapabilities.h>
#include <sbMemoryUtils.h>
#include <sbStringUtils.h>

// Mozilla includes
#include <nsComponentManagerUtils.h>
#include <nsIFile.h>
#include <nsIDOMDocument.h>
#include <nsIDOMNamedNodeMap.h>
#include <nsIDOMNode.h>
#include <nsIDOMNodeList.h>
#include <nsIMutableArray.h>
#include <nsIScriptSecurityManager.h>
#include <nsIXMLHttpRequest.h>
#include <nsMemory.h>
#include <nsServiceManagerUtils.h>
#include <nsTArray.h>
#include <nsThreadUtils.h>

#define SB_DEVICE_CAPS_ELEMENT "devicecaps"
#define SB_DEVICE_CAPS_NS "http://songbirdnest.com/devicecaps/1.0"

sbDeviceXMLCapabilities::sbDeviceXMLCapabilities(
  nsIDOMDocument* aDocument) :
    mDocument(aDocument),
    mDeviceCaps(nsnull),
    mHasCapabilities(PR_FALSE)
{
  NS_ASSERTION(mDocument, "no XML document provided");
}

sbDeviceXMLCapabilities::~sbDeviceXMLCapabilities()
{
}

nsresult
sbDeviceXMLCapabilities::AddFunctionType(PRUint32 aFunctionType)
{
  return mDeviceCaps->SetFunctionTypes(&aFunctionType, 1);
}

nsresult
sbDeviceXMLCapabilities::AddContentType(PRUint32 aFunctionType,
                                        PRUint32 aContentType)
{
  return mDeviceCaps->AddContentTypes(aFunctionType, &aContentType, 1);
}

nsresult
sbDeviceXMLCapabilities::AddFormat(PRUint32 aContentType,
                                   nsAString const & aFormat)
{
  nsCString const & formatString = NS_LossyConvertUTF16toASCII(aFormat);
  char const * format = formatString.BeginReading();
  return mDeviceCaps->AddFormats(aContentType, &format, 1);
}

nsresult
sbDeviceXMLCapabilities::Read(sbIDeviceCapabilities * aCapabilities) {
  // This function should only be called on the main thread.
  NS_ASSERTION(NS_IsMainThread(), "not on main thread");

  nsresult rv;

  mDeviceCaps = aCapabilities;

  rv = ProcessDocument(mDocument);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

/* static */ nsresult
sbDeviceXMLCapabilities::AddCapabilities
                           (sbIDeviceCapabilities* aCapabilities,
                            const char*            aXMLCapabilitiesSpec)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aCapabilities);
  NS_ENSURE_ARG_POINTER(aXMLCapabilitiesSpec);

  // Function variables.
  nsresult rv;

  // Create an XMLHttpRequest object.
  nsCOMPtr<nsIXMLHttpRequest>
    xmlHttpRequest = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIScriptSecurityManager> ssm =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> principal;
  rv = ssm->GetSystemPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = xmlHttpRequest->Init(principal, nsnull, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the device capabilities file.
  rv = xmlHttpRequest->OpenRequest(NS_LITERAL_CSTRING("GET"),
                                   nsCString(aXMLCapabilitiesSpec),
                                   PR_FALSE,                  // async
                                   SBVoidString(),            // user
                                   SBVoidString());           // password
  NS_ENSURE_SUCCESS(rv, rv);
  rv = xmlHttpRequest->Send(nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the device capabilities document.
  nsCOMPtr<nsIDOMDocument> deviceCapabilitiesDocument;
  rv = xmlHttpRequest->GetResponseXML
                         (getter_AddRefs(deviceCapabilitiesDocument));
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the device capabilities.
  nsCOMPtr<sbIDeviceCapabilities> deviceCapabilities =
    do_CreateInstance(SONGBIRD_DEVICECAPABILITIES_CONTRACTID);
  sbDeviceXMLCapabilities xmlCapabilities(deviceCapabilitiesDocument);
  rv = xmlCapabilities.Read(deviceCapabilities);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deviceCapabilities->InitDone();
  NS_ENSURE_SUCCESS(rv, rv);

  // Add any device capabilities from the device capabilities document.
  if (xmlCapabilities.HasCapabilities()) {
    rv = aCapabilities->AddCapabilities(deviceCapabilities);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
sbDeviceXMLCapabilities::ProcessDocument(nsIDOMDocument * aDocument)
{
  nsresult rv;

  nsCOMPtr<nsIDOMNodeList> domNodes;
  rv = aDocument->GetElementsByTagNameNS(NS_LITERAL_STRING(SB_DEVICE_CAPS_NS),
                                         NS_LITERAL_STRING(SB_DEVICE_CAPS_ELEMENT),
                                         getter_AddRefs(domNodes));

  NS_ENSURE_SUCCESS(rv, rv);

  if (domNodes) {
    PRUint32 nodeCount;
    rv = domNodes->GetLength(&nodeCount);
    NS_ENSURE_SUCCESS(rv, rv);
    bool capsFound = false;
    nsCOMPtr<nsIDOMNode> domNode;
    for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
      rv = domNodes->Item(nodeIndex, getter_AddRefs(domNode));
      NS_ENSURE_SUCCESS(rv, rv);

      nsString name;
      rv = domNode->GetNodeName(name);
      NS_ENSURE_SUCCESS(rv, rv);

      if (name.EqualsLiteral("devicecaps")) {
        capsFound = true;
        rv = ProcessDeviceCaps(domNode);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    if (capsFound) {
      mHasCapabilities = PR_TRUE;
    }
  }

  return NS_OK;
}

nsresult
sbDeviceXMLCapabilities::ProcessDeviceCaps(nsIDOMNode * aDevCapNode)
{
  nsCOMPtr<nsIDOMNodeList> nodes;
  nsresult rv = aDevCapNode->GetChildNodes(getter_AddRefs(nodes));
  if (nodes) {
    PRUint32 nodeCount;
    rv = nodes->GetLength(&nodeCount);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDOMNode> domNode;
    for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
      rv = nodes->Item(nodeIndex, getter_AddRefs(domNode));
      NS_ENSURE_SUCCESS(rv, rv);

      nsString name;
      rv = domNode->GetNodeName(name);
      NS_ENSURE_SUCCESS(rv, rv);

      if (name.Equals(NS_LITERAL_STRING("audio"))) {
        rv = ProcessAudio(domNode);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else if (name.Equals(NS_LITERAL_STRING("image"))) {
        rv = ProcessImage(domNode);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else if (name.Equals(NS_LITERAL_STRING("video"))) {
        rv = ProcessVideo(domNode);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else if (name.Equals(NS_LITERAL_STRING("playlist"))) {
        rv = ProcessPlaylist(domNode);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
  return NS_OK;
}

/**
 * Helper class that is used to retrieve attribute values from DOM node
 */
class sbDOMNodeAttributes
{
public:
  /**
   * Initialize the object with a DOM node
   */
  sbDOMNodeAttributes(nsIDOMNode * aNode)
  {
    if (aNode) {
      aNode->GetAttributes(getter_AddRefs(mAttributes));
    }
  }
  /**
   * Retrieves a string attribute value
   * \param aName The name of the attribute
   * \param aValue The string to receive the value
   */
  nsresult
  GetValue(nsAString const & aName,
           nsAString & aValue)
  {
    NS_ENSURE_TRUE(mAttributes, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDOMNode> node;
    nsresult rv = mAttributes->GetNamedItem(aName, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    if (node) {
      rv = node->GetNodeValue(aValue);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }

    return NS_ERROR_NOT_AVAILABLE;
  }

  /**
   * Retrieve an integer attribute value
   * \param aName The name of the attribute
   * \param aValue The integer to receive the value
   */
  nsresult
  GetValue(nsAString const & aName,
           PRInt32 & aValue)
  {
    nsString value;
    nsresult rv = GetValue(aName, value);
    // If not found then just return the error, no need to log
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      return rv;
    }
    NS_ENSURE_SUCCESS(rv, rv);

    aValue = value.ToInteger(&rv, 10);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }
private:
  nsCOMPtr<nsIDOMNamedNodeMap> mAttributes;
};

/**
 * Gets the content value of the node
 * \param aValueNode The DOM node to get the value
 * \param aValue Receives the value from the node
 */
static nsresult
GetNodeValue(nsIDOMNode * aValueNode, nsAString & aValue)
{
  NS_ENSURE_ARG_POINTER(aValueNode);

  nsCOMPtr<nsIDOMNodeList> nodes;
  nsresult rv = aValueNode->GetChildNodes(getter_AddRefs(nodes));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 nodeCount;
  rv = nodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nodeCount > 0) {
    nsCOMPtr<nsIDOMNode> node;
    rv = nodes->Item(0, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = node->GetNodeValue(aValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

/**
 * Builds a range object from a range node
 * \param aRangeNode The range node to use to build the range object
 * \param aRange The returned range
 */
static nsresult
BuildRange(nsIDOMNode * aRangeNode, sbIDevCapRange ** aRange)
{
  NS_ENSURE_ARG_POINTER(aRangeNode);
  NS_ENSURE_ARG_POINTER(aRange);

  nsresult rv;
  nsCOMPtr<sbIDevCapRange> range =
    do_CreateInstance(SB_IDEVCAPRANGE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNodeList> nodes;
  rv = aRangeNode->GetChildNodes(getter_AddRefs(nodes));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 nodeCount;
  rv = nodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    nsCOMPtr<nsIDOMNode> node;
    rv = nodes->Item(nodeIndex, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString name;
    rv = node->GetNodeName(name);
    NS_ENSURE_SUCCESS(rv, rv);

    if (name.EqualsLiteral("value")) {
      nsString bitRate;
      rv = GetNodeValue(node, bitRate);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt32 bitRateValue = bitRate.ToInteger(&rv, 10);
      if (NS_SUCCEEDED(rv)) {
        rv = range->AddValue(bitRateValue);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    else if (name.EqualsLiteral("range")) {
      sbDOMNodeAttributes attributes(node);
      PRInt32 min = 0;
      rv = attributes.GetValue(NS_LITERAL_STRING("min"), min);
      if (rv != NS_ERROR_NOT_AVAILABLE) { // not found error is ok, leave at 0
         NS_ENSURE_SUCCESS(rv, rv);
      }

      PRInt32 max = 0;
      rv = attributes.GetValue(NS_LITERAL_STRING("max"), max);
      if (rv != NS_ERROR_NOT_AVAILABLE) { // not found error is ok, leave at 0
        NS_ENSURE_SUCCESS(rv, rv);
      }

      PRInt32 step = 0;
      rv = attributes.GetValue(NS_LITERAL_STRING("step"), step);
      if (rv != NS_ERROR_NOT_AVAILABLE) { // not found error is ok, leave at 0
        NS_ENSURE_SUCCESS(rv, rv);
      }

      rv = range->Initialize(min, max, step);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  range.forget(aRange);

  return NS_OK;
}

static
nsresult
GetStringValues(nsIDOMNode * aDOMNode, char**& aValues, PRUint32 & aCount)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);

  nsCOMPtr<nsIDOMNodeList> domNodes;
  nsresult rv = aDOMNode->GetChildNodes(getter_AddRefs(domNodes));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!domNodes) {
    return NS_OK;
  }

  PRUint32 nodeCount;
  rv = domNodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nodeCount == 0) {
    return NS_OK;
  }

  nsTArray<nsString> strings;
  nsCOMPtr<nsIDOMNode> domNode;
  for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    rv = domNodes->Item(nodeIndex, getter_AddRefs(domNode));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString name;
    rv = domNode->GetNodeName(name);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!name.EqualsLiteral("value")) {
      continue;
    }

    nsString value;
    rv = GetNodeValue(domNode, value);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(strings.AppendElement(value), NS_ERROR_OUT_OF_MEMORY);
  }

  aCount = strings.Length();
  aValues = reinterpret_cast<char**>(nsMemory::Alloc(aCount * sizeof(char *)));
  for (PRUint32 index = 0; index < aCount; ++index) {
    nsString const & value = strings[index];
    aValues[index] = reinterpret_cast<char*>(
                                nsMemory::Alloc(value.Length() + 1));
    strcpy(aValues[index], ::NS_LossyConvertUTF16toASCII(value).BeginReading());
  }
  return NS_OK;
}

nsresult
sbDeviceXMLCapabilities::ProcessAudio(nsIDOMNode * aAudioNode)
{
  NS_ENSURE_ARG_POINTER(aAudioNode);

  nsCOMPtr<nsIDOMNodeList> domNodes;
  nsresult rv = aAudioNode->GetChildNodes(getter_AddRefs(domNodes));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!domNodes) {
    return NS_OK;
  }

  PRUint32 nodeCount;
  rv = domNodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nodeCount == 0) {
    return NS_OK;
  }
  rv = AddFunctionType(sbIDeviceCapabilities::FUNCTION_AUDIO_PLAYBACK);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddContentType(sbIDeviceCapabilities::FUNCTION_AUDIO_PLAYBACK,
                      sbIDeviceCapabilities::CONTENT_AUDIO);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> domNode;
  for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    rv = domNodes->Item(nodeIndex, getter_AddRefs(domNode));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString name;
    rv = domNode->GetNodeName(name);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!name.EqualsLiteral("format")) {
      continue;
    }

    sbDOMNodeAttributes attributes(domNode);

    nsString mimeType;
    rv = attributes.GetValue(NS_LITERAL_STRING("mime"),
                             mimeType);
    NS_ENSURE_SUCCESS(rv, rv);

    nsString container;
    rv = attributes.GetValue(NS_LITERAL_STRING("container"),
                             container);
    if (rv != NS_ERROR_NOT_AVAILABLE) { // not found error is ok, leave blank
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsString codec;
    rv = attributes.GetValue(NS_LITERAL_STRING("codec"),
                             codec);
    if (rv != NS_ERROR_NOT_AVAILABLE) { // not found error is ok, leave blank
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIDOMNodeList> ranges;
    rv = domNode->GetChildNodes(getter_AddRefs(ranges));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 rangeCount;
    rv = ranges->GetLength(&rangeCount);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<sbIDevCapRange> bitRates;
    nsCOMPtr<sbIDevCapRange> sampleRates;
    nsCOMPtr<sbIDevCapRange> channels;
    for (PRUint32 rangeIndex = 0; rangeIndex < rangeCount; ++rangeIndex) {
      nsCOMPtr<nsIDOMNode> range;
      rv = ranges->Item(rangeIndex, getter_AddRefs(range));
      NS_ENSURE_SUCCESS(rv, rv);

      nsString rangeName;
      rv = range->GetNodeName(rangeName);
      NS_ENSURE_SUCCESS(rv, rv);

      if (rangeName.EqualsLiteral("bitrates")) {
        rv = BuildRange(range, getter_AddRefs(bitRates));
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else if (rangeName.EqualsLiteral("samplerates")) {
        rv = BuildRange(range, getter_AddRefs(sampleRates));
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else if (rangeName.EqualsLiteral("channels")) {
        rv = BuildRange(range, getter_AddRefs(channels));
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    nsCOMPtr<sbIAudioFormatType> formatType =
      do_CreateInstance(SB_IAUDIOFORMATTYPE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = formatType->Initialize(NS_ConvertUTF16toUTF8(container),
                                NS_ConvertUTF16toUTF8(codec),
                                bitRates,
                                sampleRates,
                                channels,
                                nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = AddFormat(sbIDeviceCapabilities::CONTENT_AUDIO, mimeType);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDeviceCaps->AddFormatType(mimeType, formatType);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
sbDeviceXMLCapabilities::ProcessImageSizes(
                                         nsIDOMNode * aImageSizeNode,
                                         nsIMutableArray * aImageSizes)
{
  NS_ENSURE_ARG_POINTER(aImageSizeNode);
  NS_ENSURE_ARG_POINTER(aImageSizes);

  nsresult rv;

  nsCOMPtr<nsIDOMNodeList> nodes;
  rv = aImageSizeNode->GetChildNodes(getter_AddRefs(nodes));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 nodeCount;
  rv = nodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_NAMED_LITERAL_STRING(WIDTH, "width");
  NS_NAMED_LITERAL_STRING(HEIGHT, "height");

  for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    nsCOMPtr<nsIDOMNode> node;
    rv = nodes->Item(nodeIndex, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString name;
    rv = node->GetNodeName(name);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!name.EqualsLiteral("size")) {
      continue;
    }
    sbDOMNodeAttributes attributes(node);

    nsCOMPtr<sbIImageSize> imageSize =
      do_CreateInstance(SB_IMAGESIZE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 width;
    rv = attributes.GetValue(WIDTH, width);
    if (NS_FAILED(rv)) {
      NS_WARNING("Invalid width found in device settings file");
      continue;
    }

    PRInt32 height;
    rv = attributes.GetValue(HEIGHT, height);
    if (NS_FAILED(rv)) {
      continue;
    }
    rv = imageSize->Initialize(width, height);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aImageSizes->AppendElement(imageSize, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
sbDeviceXMLCapabilities::ProcessImage(nsIDOMNode * aImageNode)
{
  NS_ENSURE_ARG_POINTER(aImageNode);

  nsCOMPtr<nsIDOMNodeList> domNodes;
  nsresult rv = aImageNode->GetChildNodes(getter_AddRefs(domNodes));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!domNodes) {
    return NS_OK;
  }

  PRUint32 nodeCount;
  rv = domNodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nodeCount == 0) {
    return NS_OK;
  }
  rv = AddFunctionType(sbIDeviceCapabilities::FUNCTION_IMAGE_DISPLAY);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddContentType(sbIDeviceCapabilities::FUNCTION_IMAGE_DISPLAY,
                      sbIDeviceCapabilities::CONTENT_IMAGE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> domNode;
  for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    rv = domNodes->Item(nodeIndex, getter_AddRefs(domNode));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString name;
    rv = domNode->GetNodeName(name);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!name.EqualsLiteral("format")) {
      continue;
    }

    sbDOMNodeAttributes attributes(domNode);

    nsString mimeType;
    rv = attributes.GetValue(NS_LITERAL_STRING("mime"),
                             mimeType);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMutableArray> imageSizes =
      do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1",
                        &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<sbIDevCapRange> widths;
    nsCOMPtr<sbIDevCapRange> heights;

    nsCOMPtr<nsIDOMNodeList> sections;
    rv = domNode->GetChildNodes(getter_AddRefs(sections));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 sectionCount;
    rv = sections->GetLength(&sectionCount);
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRUint32 sectionIndex = 0;
         sectionIndex < sectionCount;
         ++sectionIndex) {
      nsCOMPtr<nsIDOMNode> section;
      rv = sections->Item(sectionIndex, getter_AddRefs(section));
      NS_ENSURE_SUCCESS(rv, rv);

      nsString sectionName;
      rv = section->GetNodeName(sectionName);
      NS_ENSURE_SUCCESS(rv, rv);

      if (sectionName.EqualsLiteral("explicit-sizes")) {
        rv = ProcessImageSizes(section, imageSizes);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else if (sectionName.EqualsLiteral("widths")) {
        rv = BuildRange(section, getter_AddRefs(widths));
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else if (sectionName.EqualsLiteral("heights")) {
        rv = BuildRange(section, getter_AddRefs(heights));
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    nsCOMPtr<sbIImageFormatType> imageFormatType =
      do_CreateInstance(SB_IIMAGEFORMATTYPE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = imageFormatType->Initialize(NS_ConvertUTF16toUTF8(mimeType),
                                     imageSizes,
                                     widths,
                                     heights);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = AddFormat(sbIDeviceCapabilities::CONTENT_IMAGE, mimeType);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDeviceCaps->AddFormatType(mimeType, imageFormatType);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


typedef sbAutoFreeXPCOMArrayByRef<char**> sbAutoStringArray;

nsresult
sbDeviceXMLCapabilities::ProcessVideoStream(nsIDOMNode* aVideoStreamNode,
                                            sbIDevCapVideoStream** aVideoStream)
{
  // Retrieve the child nodes for the video node
  nsCOMPtr<nsIDOMNodeList> domNodes;
  nsresult rv = aVideoStreamNode->GetChildNodes(getter_AddRefs(domNodes));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!domNodes) {
    return NS_OK;
  }

  // See if there are any child nodes, leave if not
  PRUint32 nodeCount;
  rv = domNodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);
  if (nodeCount == 0) {
    return NS_OK;
  }

  nsString type;
  sbDOMNodeAttributes attributes(aVideoStreamNode);

  // Failure is an option, leaves type blank
  attributes.GetValue(NS_LITERAL_STRING("type"), type);

  nsCOMPtr<nsIMutableArray> sizes =
    do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1",
                      &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<sbIDevCapRange> widths;
  nsCOMPtr<sbIDevCapRange> heights;
  nsCOMPtr<sbIDevCapRange> bitRates;
  char ** videoPARs = nsnull;
  PRUint32 videoPARCount = 0;
  sbAutoStringArray videPARAuto(videoPARCount, videoPARs);
  char ** frameRates = nsnull;
  PRUint32 frameRateCount = 0;
  sbAutoStringArray frameRatesAuto(frameRateCount, frameRates);
  nsCOMPtr<nsIDOMNode> domNode;
  for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    rv = domNodes->Item(nodeIndex, getter_AddRefs(domNode));
    NS_ENSURE_SUCCESS(rv, rv);
    nsString name;
    rv = domNode->GetNodeName(name);
    if (NS_FAILED(rv)) {
      continue;
    }
    if (name.Equals(NS_LITERAL_STRING("explicit-sizes"))) {
      rv = ProcessImageSizes(domNode, sizes);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (name.Equals(NS_LITERAL_STRING("widths"))) {
      rv = BuildRange(domNode, getter_AddRefs(widths));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (name.Equals(NS_LITERAL_STRING("heights"))) {
      rv = BuildRange(domNode, getter_AddRefs(heights));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (name.Equals(NS_LITERAL_STRING("videoPARs"))) {
      if (videoPARs) {
        nsMemory::Free(videoPARs);
      }
      rv = GetStringValues(domNode, videoPARs, videoPARCount);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (name.Equals(NS_LITERAL_STRING("frame-rates"))) {
      if (frameRates) {
        nsMemory::Free(frameRates);
      }
      rv = GetStringValues(domNode, frameRates, frameRateCount);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (name.Equals(NS_LITERAL_STRING("bit-rates"))) {
      rv = BuildRange(domNode, getter_AddRefs(bitRates));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsCOMPtr<sbIDevCapVideoStream> videoStream =
    do_CreateInstance(SB_IDEVCAPVIDEOSTREAM_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = videoStream->Initialize(NS_ConvertUTF16toUTF8(type),
                               sizes,
                               widths,
                               heights,
                               videoPARCount,
                               const_cast<char const **>(videoPARs),
                               frameRateCount,
                               const_cast<char const **>(frameRates),
                               bitRates);
  NS_ENSURE_SUCCESS(rv, rv);

  videoStream.forget(aVideoStream);

  return NS_OK;
}

nsresult
sbDeviceXMLCapabilities::ProcessAudioStream(nsIDOMNode* aAudioStreamNode,
                                            sbIDevCapAudioStream** aAudioStream)
{
  // Retrieve the child nodes for the video node
  nsCOMPtr<nsIDOMNodeList> domNodes;
  nsresult rv = aAudioStreamNode->GetChildNodes(getter_AddRefs(domNodes));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!domNodes) {
    return NS_OK;
  }

  // See if there are any child nodes, leave if not
  PRUint32 nodeCount;
  rv = domNodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);
  if (nodeCount == 0) {
    return NS_OK;
  }

  nsString type;
  sbDOMNodeAttributes attributes(aAudioStreamNode);

  // Failure is an option, leaves type blank
  attributes.GetValue(NS_LITERAL_STRING("type"), type);

  nsCOMPtr<sbIDevCapRange> bitRates;
  nsCOMPtr<sbIDevCapRange> sampleRates;
  nsCOMPtr<sbIDevCapRange> channels;

  nsCOMPtr<nsIDOMNode> domNode;
  for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    rv = domNodes->Item(nodeIndex, getter_AddRefs(domNode));
    NS_ENSURE_SUCCESS(rv, rv);
    nsString name;
    rv = domNode->GetNodeName(name);
    if (NS_FAILED(rv)) {
      continue;
    }
    if (name.Equals(NS_LITERAL_STRING("bit-rates"))) {
      rv = BuildRange(domNode, getter_AddRefs(bitRates));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (name.Equals(NS_LITERAL_STRING("sample-rates"))) {
      rv = BuildRange(domNode, getter_AddRefs(sampleRates));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (name.Equals(NS_LITERAL_STRING("channels"))) {
      rv = BuildRange(domNode, getter_AddRefs(channels));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsCOMPtr<sbIDevCapAudioStream> audioStream =
    do_CreateInstance(SB_IDEVCAPAUDIOSTREAM_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = audioStream->Initialize(NS_ConvertUTF16toUTF8(type),
                               bitRates,
                               sampleRates,
                               channels);
  NS_ENSURE_SUCCESS(rv, rv);

  audioStream.forget(aAudioStream);

  return NS_OK;
}

nsresult
sbDeviceXMLCapabilities::ProcessVideoFormat(nsIDOMNode* aVideoFormatNode)
{
  nsresult rv;

  sbDOMNodeAttributes attributes(aVideoFormatNode);

  nsString containerType;
  rv = attributes.GetValue(NS_LITERAL_STRING("container-type"),
                           containerType);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  // Retrieve the child nodes for the video node
  nsCOMPtr<nsIDOMNodeList> domNodes;
  rv = aVideoFormatNode->GetChildNodes(getter_AddRefs(domNodes));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!domNodes) {
    return NS_OK;
  }

  // See if there are any child nodes, leave if not
  PRUint32 nodeCount;
  rv = domNodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbIDevCapVideoStream> videoStream;
  nsCOMPtr<sbIDevCapAudioStream> audioStream;
  nsCOMPtr<nsIDOMNode> domNode;
  for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    rv = domNodes->Item(nodeIndex, getter_AddRefs(domNode));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString name;
    rv = domNode->GetNodeName(name);
    if (NS_FAILED(rv)) {
      continue;
    }
    if (name.Equals(NS_LITERAL_STRING("video-stream"))) {
      ProcessVideoStream(domNode, getter_AddRefs(videoStream));
    }
    else if (name.Equals(NS_LITERAL_STRING("audio-stream"))) {
      ProcessAudioStream(domNode, getter_AddRefs(audioStream));
    }
  }

  nsCOMPtr<sbIVideoFormatType> videoFormat =
    do_CreateInstance(SB_IVIDEOFORMATTYPE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = videoFormat->Initialize(NS_ConvertUTF16toUTF8(containerType),
                               videoStream,
                               audioStream);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddFormat(sbIDeviceCapabilities::CONTENT_VIDEO, containerType);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDeviceCaps->AddFormatType(containerType, videoFormat);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbDeviceXMLCapabilities::ProcessVideo(nsIDOMNode * aVideoNode)
{
  NS_ENSURE_ARG_POINTER(aVideoNode);

  // Retrieve the child nodes for the video node
  nsCOMPtr<nsIDOMNodeList> domNodes;
  nsresult rv = aVideoNode->GetChildNodes(getter_AddRefs(domNodes));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!domNodes) {
    return NS_OK;
  }

  // See if there are any child nodes, leave if not
  PRUint32 nodeCount;
  rv = domNodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);
  if (nodeCount == 0) {
    return NS_OK;
  }

  rv = AddFunctionType(sbIDeviceCapabilities::FUNCTION_VIDEO_PLAYBACK);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddContentType(sbIDeviceCapabilities::FUNCTION_VIDEO_PLAYBACK,
                      sbIDeviceCapabilities::CONTENT_VIDEO);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> domNode;
  for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    rv = domNodes->Item(nodeIndex, getter_AddRefs(domNode));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString name;
    rv = domNode->GetNodeName(name);
    if (NS_FAILED(rv)) {
      continue;
    }

    // Ignore things we don't know about
    if (!name.EqualsLiteral("format")) {
      continue;
    }

    ProcessVideoFormat(domNode);
  }
  return NS_OK;
}

nsresult
sbDeviceXMLCapabilities::ProcessPlaylist(nsIDOMNode * aPlaylistNode)
{
  NS_ENSURE_ARG_POINTER(aPlaylistNode);

  nsCOMPtr<nsIDOMNodeList> domNodes;
  nsresult rv = aPlaylistNode->GetChildNodes(getter_AddRefs(domNodes));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!domNodes) {
    return NS_OK;
  }

  PRUint32 nodeCount;
  rv = domNodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nodeCount == 0) {
    return NS_OK;
  }
  //XXXeps it shouldn't be specific to audio. MEDIA_PLAYBACK???
  rv = AddFunctionType(sbIDeviceCapabilities::FUNCTION_AUDIO_PLAYBACK);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddContentType(sbIDeviceCapabilities::FUNCTION_AUDIO_PLAYBACK,
                      sbIDeviceCapabilities::CONTENT_PLAYLIST);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> domNode;
  for (PRUint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    rv = domNodes->Item(nodeIndex, getter_AddRefs(domNode));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString name;
    rv = domNode->GetNodeName(name);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!name.EqualsLiteral("format")) {
      continue;
    }

    sbDOMNodeAttributes attributes(domNode);

    nsString mimeType;
    rv = attributes.GetValue(NS_LITERAL_STRING("mime"),
                             mimeType);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = AddFormat(sbIDeviceCapabilities::CONTENT_PLAYLIST, mimeType);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


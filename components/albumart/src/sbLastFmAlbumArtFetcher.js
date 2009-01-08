/*
//
// BEGIN SONGBIRD GPL
//
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2008 POTI, Inc.
// http://songbirdnest.com
//
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the "GPL").
//
// Software distributed under the License is distributed
// on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either
// express or implied. See the GPL for the specific language
// governing rights and limitations.
//
// You should have received a copy of the GPL along with this
// program. If not, go to http://www.gnu.org/licenses/gpl.html
// or write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// END SONGBIRD GPL
//
*/


/* This XPCOM service knows how to ask Last.fm where to find art for 
 * music albums. 
 */

const Cc = Components.classes;
const CC = Components.Constructor;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

// The root of our preferences branch
const PREF_BRANCH = "songbird.albumart.lastfm.";

// Importing helper modules
Cu.import('resource://app/jsmodules/sbProperties.jsm');
Cu.import("resource://app/jsmodules/StringUtils.jsm");
Cu.import("resource://app/jsmodules/ArrayConverter.jsm");
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

/**
 * Since we can't use the FUEL components until after all other components have
 * been loaded we define a lazy getter here for when we need it.
 */
__defineGetter__("Application", function() {
  delete this.Application;
  return this.Application = Cc["@mozilla.org/fuel/application;1"]
                              .getService(Ci.fuelIApplication);
});

// This is the fetcher module, you will need to change the name
// sbLastFMAlbumArtFetcher to you own in all instances.
function sbLastFMAlbumArtFetcher() {
  // Use the last FM web api to make things easier.
  this._lastFMWebApi = Cc['@songbirdnest.com/Songbird/webservices/last-fm;1']
                         .getService(Ci.sbILastFmWebServices);
};
sbLastFMAlbumArtFetcher.prototype = {
  // XPCOM Magic
  className: 'sbLastFMAlbumArtFetcher',
  classDescription: 'LastFM Album Cover Fetcher',
  classID: Components.ID('{7386965b-f313-49d7-9b55-f9f9b9b51875}'),
  contractID: '@songbirdnest.com/Songbird/album-art/lastfm-fetcher;1',
  _xpcom_categories: [{
    category: "songbird-album-art-fetcher"
  }],

  // Variables
  _shutdown: false,
  _albumArtSourceList: null,

  // These are a bunch of getters for attributes in the sbICoverFetcher.idl
  get shortName() {
    return "lastfm"; // Change this to something that represents your fetcher
  },
  
  // These next few use the .properties file to get the information
  get name() {
    return SBString(PREF_BRANCH + "name", null, null);
  },
  
  get description() {
    return SBString(PREF_BRANCH + "description", null, null);
  },
  
  get isLocal() {
    return false;
  },
  
  // These are preference settings
  get priority() {
    return Application.prefs.getValue(PREF_BRANCH + "priority", 0);
  },
  set priority(aNewVal) {
    return Application.prefs.setValue(PREF_BRANCH + "priority", aNewVal);
  },
  
  get isEnabled() {
    return Application.prefs.getValue(PREF_BRANCH + "enabled", false);
  },
  set isEnabled(aNewVal) {
    return Application.prefs.setValue(PREF_BRANCH + "enabled", aNewVal);
  },
  
  get albumArtSourceList() {
    return this._albumArtSourceList;
  },
  set albumArtSourceList(aNewVal) {
    this._albumArtSourceList = aNewVal;
  },

  /*********************************
   * sbIAlbumArtFetcher
   ********************************/
  fetchAlbumArtForMediaItem: function (aMediaItem, aListener) {
    var arguments = Cc["@mozilla.org/hash-property-bag;1"]
                    .createInstance(Ci.nsIWritablePropertyBag2);
    arguments.setPropertyAsACString("track",
                              aMediaItem.getProperty(SBProperties.trackName));
    arguments.setPropertyAsACString("artist",
                              aMediaItem.getProperty(SBProperties.artistName));

    var apiResponse = function response(success, xml) {
      // Abort if we are shutting down
      if (this._shutdown) {
        return;
      }
      
      // Failed to get a good response back from the server :(
      if (!success) {
        aListener.onResult(null, aMediaItem);
        return;
      }
      
      var foundCover;
      var imageSizes = ['large', 'medium', 'small'];
      // Use XPath to parse out the image, we want to find the first image
      // in the order of the imageSizes array above.
      var nsResolver = xml.createNSResolver(xml.ownerDocument == null ?
                                            xml.documentElement :
                                            xml.ownerDocument.documentElement);
      for (var iSize = 0; iSize < imageSizes.length; iSize++) {
        var result = xml.evaluate("//image[@size='" + imageSizes[iSize] + "']",
                                  xml,
                                  nsResolver,
                                  7,  //XPathResult.ORDERED_NODE_SNAPSHOT_TYPE,
                                  null);
        if (result.snapshotLength > 0) {
          foundCover = result.snapshotItem(0).textContent;
          break;
        }
      }
    
      if (foundCover == null) {
        // No cover so indicate failure
        aListener.onResult(null, aMediaItem);
      } else {
        // Found the cover so we need to download it
        var albumArtDownloader = Cc["@songbirdnest.com/Songbird/album-art/downloader;1"]
                                   .createInstance(Ci.sbIAlbumArtDownloader);
        albumArtDownloader.downloadImage(foundCover, aMediaItem, aListener);
      }
    };

    this._lastFMWebApi.apiCall("track.getInfo",  // method
                               arguments,        // Property bag of strings
                               apiResponse);     // Callback

  },
  
  shutdown: function () {
    this._shutdown = true;
  },

  /*********************************
   * nsISupports
   ********************************/
  QueryInterface: XPCOMUtils.generateQI([Ci.sbIAlbumArtFetcher])
}

// This is for XPCOM to register this Fetcher as a module
function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule([sbLastFMAlbumArtFetcher]);
}

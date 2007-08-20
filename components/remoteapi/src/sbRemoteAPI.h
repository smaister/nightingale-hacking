/*
//
// BEGIN SONGBIRD GPL
//
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2007 POTI, Inc.
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

#ifndef __SB_REMOTE_API_H__
#define __SB_REMOTE_API_H__

#include <sbISecurityAggregator.h>
#include <sbISecurityMixin.h>

#ifndef LOG
#define LOG(args) /* nothing */
#endif

#define SB_DECL_SECURITYCHECKEDCOMP_INIT virtual nsresult Init();

#define SB_IMPL_SECURITYCHECKEDCOMP_INIT(_class)                              \
nsresult                                                                      \
_class::Init()                                                                \
{                                                                             \
  LOG(( "%s::Init()", #_class ));                                             \
  nsresult rv;                                                                \
  nsCOMPtr<sbISecurityMixin> mixin =                                          \
     do_CreateInstance( "@songbirdnest.com/remoteapi/security-mixin;1", &rv );\
  NS_ENSURE_SUCCESS(rv, rv);                                                  \
  /* Get the list of IIDs to pass to the security mixin */                    \
  nsIID **iids;                                                               \
  PRUint32 iidCount;                                                          \
  GetInterfaces( &iidCount, &iids );                                          \
  /* initialize our mixin with approved interfaces, methods, properties */    \
  rv = mixin->Init( (sbISecurityAggregator*)this,                             \
                    ( const nsIID** )iids, iidCount,                          \
                    sPublicMethods, NS_ARRAY_LENGTH(sPublicMethods),          \
                    sPublicRProperties, NS_ARRAY_LENGTH(sPublicRProperties),  \
                    sPublicWProperties, NS_ARRAY_LENGTH(sPublicWProperties) );\
  NS_ENSURE_SUCCESS( rv, rv );                                                \
  mSecurityMixin = do_QueryInterface( mixin, &rv );                           \
  NS_ENSURE_SUCCESS( rv, rv );                                                \
  return NS_OK;                                                               \
}

#endif // __SB_REMOTE_API_H__


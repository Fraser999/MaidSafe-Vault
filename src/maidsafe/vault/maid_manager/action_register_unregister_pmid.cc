/* Copyright 2013 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#include "maidsafe/vault/maid_manager/action_register_unregister_pmid.h"

#include "maidsafe/common/error.h"

#include "maidsafe/vault/maid_manager/action_register_unregister_pmid.pb.h"
#include "maidsafe/vault/maid_manager/metadata.h"


namespace maidsafe {

namespace vault {

template<>
const nfs::MessageAction ActionRegisterUnregisterPmid<false>::kActionId =
    nfs::MessageAction::kRegisterPmid;

template<>
const nfs::MessageAction ActionRegisterUnregisterPmid<true>::kActionId =
    nfs::MessageAction::kUnregisterPmid;

template<>
void ActionRegisterUnregisterPmid<false>::operator()(
    boost::optional<MaidManagerMetadata>& value) const {
  if (!value)
    ThrowError(VaultErrors::no_such_account);
  value->RegisterPmid(kPmidRegistration);
}


template<>
void ActionRegisterUnregisterPmid<true>::operator()(
    boost::optional<MaidManagerMetadata>& value) const {
  if (!value)
    ThrowError(VaultErrors::no_such_account);
  value->UnregisterPmid(kPmidRegistration);
}

}  // namespace vault

}  // namespace maidsafe

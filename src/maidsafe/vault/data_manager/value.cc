/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include "maidsafe/vault/data_manager/value.h"

#include <string>

//#include "maidsafe/vault/utils.h"

namespace maidsafe {

namespace vault {

DataManagerValue::DataManagerValue(const serialised_type& serialised_metadata_value)
    : subscribers_(0), size_(0), store_failures_(0), online_pmids_(), offline_pmids_() {
  protobuf::DataManagerValue metadata_value_proto;
  if (!metadata_value_proto.ParseFromString(serialised_metadata_value->string())) {
    LOG(kError) << "Failed to read or parse serialised metadata value";
    ThrowError(CommonErrors::parsing_error);
  } else {
    if ((metadata_value_proto.subscribers() < 1) || (metadata_value_proto.size() <= 0)) {
      LOG(kError) << "Invalid parameters";
      ThrowError(CommonErrors::invalid_parameter);
    }
    subscribers_ = metadata_value_proto.subscribers();
    size_ = metadata_value_proto.size();
    store_failures_ = metadata_value_proto.store_failures();

    for (auto& i : metadata_value_proto.online_pmid_name())
      online_pmids_.insert(PmidName(Identity(i)));
    for (auto& i : metadata_value_proto.offline_pmid_name())
      offline_pmids_.insert(PmidName(Identity(i)));
    if (online_pmids_.size() + offline_pmids_.size() < 1) {
      LOG(kError) << "Invalid online/offline pmids";
      ThrowError(CommonErrors::invalid_parameter);
    }
  }
}

DataManagerValue::DataManagerValue()
    : subscribers_(0), size_(0), store_failures_(0), online_pmids_(), offline_pmids_() {}

void DataManagerValue::AddPmid(const PmidName& pmid_name) {
  online_pmids_.insert(pmid_name);
  offline_pmids_.erase(pmid_name);
}

void DataManagerValue::RemovePmid(const PmidName& pmid_name) {
  if (online_pmids_.size() + offline_pmids_.size() < 4) {
    LOG(kError) << "RemovePmid not allowed";
    ThrowError(CommonErrors::invalid_parameter);  // TODO add error - not_allowed
  }
  online_pmids_.erase(pmid_name);
  offline_pmids_.erase(pmid_name);
}

void DataManagerValue::IncrementSubscribers() { ++subscribers_; }

int64_t DataManagerValue::DecrementSubscribers() {
  --subscribers_;
  return subscribers_;
}

void DataManagerValue::SetPmidOnline(const PmidName& pmid_name) {
  auto deleted = offline_pmids_.erase(pmid_name);
  if (deleted == 1) {
    online_pmids_.insert(pmid_name);
  } else {
    LOG(kError) << "Invalid Pmid reported";
    ThrowError(CommonErrors::invalid_parameter);
  }
}

void DataManagerValue::SetPmidOffline(const PmidName& pmid_name) {
  auto deleted = online_pmids_.erase(pmid_name);
  if (deleted == 1) {
    offline_pmids_.insert(pmid_name);
  } else {
    LOG(kError) << "Invalid Pmid reported";
    ThrowError(CommonErrors::invalid_parameter);
  }
}

int64_t DataManagerValue::Subscribers() const { return subscribers_; }

int32_t DataManagerValue::StoreFailures() const { return store_failures_; }

DataManagerValue::serialised_type DataManagerValue::Serialise() const {
  if (subscribers_ < 1 || size_ <= 0)
    ThrowError(CommonErrors::uninitialised);  // Cannot serialise if not a complete db value
  assert((online_pmids_.size() + offline_pmids_.size()) > 0);
  protobuf::DataManagerValue metadata_value_proto;
  metadata_value_proto.set_subscribers(subscribers_);
  metadata_value_proto.set_size(size_);
  metadata_value_proto.set_store_failures(store_failures_);
  for (const auto& i : online_pmids_)
    metadata_value_proto.add_online_pmid_name(i->string());
  for (const auto& i : offline_pmids_)
    metadata_value_proto.add_offline_pmid_name(i->string());
  assert(metadata_value_proto.IsInitialized());
  return serialised_type(NonEmptyString(metadata_value_proto.SerializeAsString()));
}

bool operator==(const DataManagerValue& lhs, const DataManagerValue& rhs) {
  return lhs.subscribers_ == rhs.subscribers_ && lhs.size_ == rhs.size_ &&
         lhs.store_failures_ == rhs.store_failures_ && lhs.online_pmids_ == rhs.online_pmids_ &&
         lhs.offline_pmids_ == rhs.offline_pmids_;
}

std::set<PmidName> DataManagerValue::Pmids(bool onlines_only) const {
  if (onlines_only)
    return online_pmids_;
  std::set<PmidName> pmids_union;
  std::set_union(std::begin(online_pmids_), std::end(online_pmids_), std::begin(offline_pmids_),
                 std::end(offline_pmids_), std::inserter(pmids_union, std::begin(pmids_union)));
  return pmids_union;
}

}  // namespace vault

}  // namespace maidsafe

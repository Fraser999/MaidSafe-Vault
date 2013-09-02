/* Copyright 2012 MaidSafe.net limited

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

#include "maidsafe/vault/pmid_node/service.h"

#include <string>
#include <chrono>

#include "maidsafe/common/utils.h"
#include "maidsafe/common/types.h"
#include "maidsafe/data_store/data_buffer.h"
#include "maidsafe/nfs/client/messages.pb.h"

#include "maidsafe/vault/pmid_manager/pmid_manager.pb.h"


namespace fs = boost::filesystem;

namespace maidsafe {

namespace vault {

namespace {

MemoryUsage mem_usage = MemoryUsage(524288000);  // 500Mb
MemoryUsage perm_usage = MemoryUsage(mem_usage / 5);
MemoryUsage cache_usage = MemoryUsage(mem_usage * 2 / 5);
// MemoryUsage mem_only_cache_usage = MemoryUsage(mem_usage * 2 / 5);
MemoryUsage mem_only_cache_usage = MemoryUsage(100);  // size in elements
//  fs::space_info space = fs::space("/tmp/vault_root_dir\\");  // FIXME  NOLINT

//  DiskUsage disk_total = DiskUsage(space.available);
//  DiskUsage permanent_size = DiskUsage(disk_total * 0.8);
//  DiskUsage cache_size = DiskUsage(disk_total * 0.1);

inline bool SenderIsConnectedVault(const nfs::Message& message, routing::Routing& routing) {
  return routing.IsConnectedVault(message.source().node_id) &&
         routing.EstimateInGroup(message.source().node_id, routing.kNodeId());
}

inline bool SenderInGroupForMetadata(const nfs::Message& message, routing::Routing& routing) {
  return routing.EstimateInGroup(message.source().node_id, NodeId(message.data().name.string()));
}

template<typename Message>
inline bool ForThisPersona(const Message& message) {
  return message.destination_persona() != nfs::Persona::kPmidNode;
}

}  // unnamed namespace


PmidNodeService::PmidNodeService(const passport::Pmid& pmid,
                                 routing::Routing& routing,
                                 const fs::path& vault_root_dir)
    : space_info_(fs::space(vault_root_dir)),
      disk_total_(space_info_.available),
      permanent_size_(disk_total_ * 4 / 5),
      cache_size_(disk_total_ / 10),
      permanent_data_store_(vault_root_dir / "pmid_node" / "permanent", DiskUsage(10000)/*perm_usage*/),  // TODO(Fraser) BEFORE_RELEASE need to read value from disk
      cache_data_store_(cache_usage, DiskUsage(cache_size_ / 2), nullptr,
                        vault_root_dir / "pmid_node" / "cache"),  // FIXME - DiskUsage  NOLINT
      mem_only_cache_(mem_only_cache_usage),
      //mem_only_cache_(mem_only_cache_usage, DiskUsage(cache_size_ / 2), nullptr,
      //                vault_root_dir / "pmid_node" / "cache"),  // FIXME - DiskUsage should be 0  NOLINT
      routing_(routing),
      accumulator_mutex_(),
      active_(),
      asio_service_(1),
      data_getter_(asio_service_, routing_) {
//  nfs_.GetElementList();  // TODO (Fraser) BEFORE_RELEASE Implementation needed
}

void PmidNodeService::HandleAccountResponses(
  const std::vector<nfs::GetPmidAccountResponseFromPmidManagerToPmidNode>& responses) {
  std::map<DataNameVariant, u_int16_t> expected_chunks;
  std::vector<protobuf::PmidAccountResponse> pmid_account_responses;
  protobuf::PmidAccountResponse pmid_account_response;
  size_t total_pmid_managers(responses.size()), total_pmid_managers_with_accounts(0);
  nfs_client::protobuf::DataNameAndContentOrReturnCode data;
  for (auto response : responses) {
    if (data.ParseFromString(response.contents)) {
      if (data.has_serialised_data_name_and_content() &&
          pmid_account_response.ParseFromString(data.serialised_data_name_and_content()))
        pmid_account_responses.push_back(pmid_account_response);
    } else {
      total_pmid_managers_with_accounts++;
    }
  }
  ApplyAccountTransfer(pmid_account_responses,
                        total_pmid_managers,
                        total_pmid_managers_with_accounts);
}

void PmidNodeService::ApplyAccountTransfer(
    const std::vector<protobuf::PmidAccountResponse>& responses,
    const size_t& total_pmidmgrs,
    const size_t& pmidmgrs_with_account) {
  std::map<DataNameVariant, uint16_t>& chunks;
  struct ChunkInfo {
    ChunkInfo(const DataNameVariant& file_name_in, const uint64_t& size_in) :
        file_name(file_name_in), size(size_in) {}
    DataNameVariant file_name;
    uint64_t size;
  };

  struct ChunkInfoComparison {
    bool operator() (const ChunkInfo& lhs, const ChunkInfo& rhs) const {
      return lhs.file_name < rhs.file_name;
    }
  };

  std::map<ChunkInfo, uint16_t, ChunkInfoComparison> expected_chunks;
  protobuf::PmidAccountDetails pmid_account_details;

  for (auto pmid_account_response : responses) {
    if (static_cast<nfs::MessageAction>(pmid_account_response.action()) ==
            nfs::MessageAction::kAccountTransfer) {
      if (pmid_account_response.status() == static_cast<int>(CommonErrors::success)) {
        pmid_account_details.ParseFromString(
            pmid_account_response.pmid_account().serialised_account_details());
        for (int index(0); index < pmid_account_details.db_entry_size(); ++index) {
          ChunkInfo chunk_info(
              ImmutableData::Name(Identity(pmid_account_details.db_entry(index).name())),
              pmid_account_details.db_entry(index).value().size());
          expected_chunks[chunk_info]++;
        }
      }
    }
  }
  for (auto iter(expected_chunks.begin()); iter != expected_chunks.end(); ++iter) {
    if ((iter->second >= routing::Parameters::node_group_size / 2 + 1) ||
        ((iter->second == routing::Parameters::node_group_size / 2)
         && (total_pmidmgrs > pmidmgrs_with_account))) {
      std::pair<DataNameVariant, int16_t> pair(iter->first.file_name, iter->second);
      chunks.insert(pair);
    }
  }
  UpdateLocalStorage(expected_chunks);
}

void PmidNodeService::UpdateLocalStorage(
    const std::map<DataNameVariant, uint16_t>& expected_files) {
  std::vector<DataNameVariant> existing_files(StoredFileNames());
  std::vector<DataNameVariant> to_be_deleted, to_be_retrieved;
  for (auto file_name : existing_files) {
    if (std::find_if(expected_files.begin(),
                     expected_files.end(),
                     [&file_name](const std::pair<DataNameVariant, bool>& expected) {
                       return expected.first == file_name;
                     }) == expected_files.end()) {
      to_be_deleted.push_back(file_name);
    }
  }
  for (auto iter(expected_files.begin()); iter != expected_files.end(); ++iter) {
    if ((std::find_if(existing_files.begin(),
                     existing_files.end(),
                     [&](const DataNameVariant& existing) {
                       return existing == iter->first;
                     }) == existing_files.end()) &&
        (iter->second >= routing::Parameters::node_group_size / 2 + 1)) {
      to_be_retrieved.push_back(iter->first);
    }
  }
  ApplyUpdateLocalStorage(to_be_deleted, to_be_retrieved);
}

void PmidNodeService::ApplyUpdateLocalStorage(const std::vector<DataNameVariant>& to_be_deleted,
                                              const std::vector<DataNameVariant>& to_be_retrieved) {
  for (auto file : to_be_deleted) {
    try {
      permanent_data_store_.Delete(file);
    }
    catch(const maidsafe_error& error) {
      LOG(kWarning) << "Error in deletion: " << error.code() << " - " << error.what();
    }
  }

  std::vector<std::future<void>> futures;
  for (auto file : to_be_retrieved) {
    GetCallerVisitor get_caller_visitor(data_getter_,
                                        futures,
                                        [this](const KeyType& key, const NonEmptyString& value) {
                                          this->permanent_data_store_(key, value);
                                        });
    boost::apply_visitor(get_caller_visitor, file);
  }

  for (auto iter(futures.begin()); iter != futures.end(); ++iter) {
    try {
      iter->wait();
    }
    catch(const maidsafe_error& error) {
      LOG(kWarning) << "Error in retreivel: " << error.code() << " - " << error.what();
    }
  }
}

std::vector<DataNameVariant> PmidNodeService::StoredFileNames() {
  std::vector<DataNameVariant> file_ids;
  fs::directory_iterator end_iter;
  auto root_path(permanent_data_store_.GetDiskPath());

  if (fs::exists(root_path) && fs::is_directory(root_path)) {
    for(fs::directory_iterator dir_iter(root_path); dir_iter != end_iter; ++dir_iter)
      if (fs::is_regular_file(dir_iter->status()))
        file_ids.push_back(PmidName(Identity(dir_iter->path().string())));
  }
  return file_ids;
}

}  // namespace vault

}  // namespace maidsafe

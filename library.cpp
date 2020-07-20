#include <vector>
#include <string>
#include <absl/strings/string_view.h>
#include <absl/container/flat_hash_map.h>

#define REDISMODULE_EXPERIMENTAL_API
#include "redismodule-6.0.4.h"

namespace {

class AssociationTable {
public:
    using ClientId = uint64_t;
    using ClientKeys = std::vector<std::string>;
public:
    void add(ClientId id, const absl::string_view key)
    {
        auto it = this->reverse_state.find(key);

        if (it == this->reverse_state.end()) {
            this->reverse_state[std::string(key)] = id;
            this->state[id].emplace_back(key);

        } else if (it->second != id) {
            this->remove_from_state(it->second, key);

            it->second = id;
            this->state[id].emplace_back(key);
        }
    }

    ClientKeys remove_client(ClientId id)
    {
        const auto it = this->state.find(id);
        if (it == this->state.cend()) {
            return ClientKeys{};
        }

        ClientKeys keys = this->state.extract(it).mapped();
        for (const auto& key: keys) {
            this->reverse_state.erase(key);
        }

        return keys;
    }

    void remove_key(const absl::string_view key)
    {
        const auto it = this->reverse_state.find(key);
        if (it == this->reverse_state.cend()) {
            return;
        }

        this->remove_from_state(it->second, key);
        this->reverse_state.erase(it);
    }

private:
    void remove_from_state(ClientId id, const absl::string_view key) {
        auto it = this->state.find(id);
        if (it == this->state.end()) {
            return;
        }

        it->second.erase(std::find(
            std::begin(it->second),
            std::end(it->second),
            key));

        if (it->second.empty()) {
            this->state.erase(it);
        }

    }

private:
    absl::flat_hash_map<ClientId, ClientKeys> state;
    absl::flat_hash_map<std::string, ClientId> reverse_state;
};

AssociationTable *association_table = nullptr;

absl::string_view
as_str_view(RedisModuleString* str)
{
    size_t size;
    const char* data = RedisModule_StringPtrLen(str, &size);
    return absl::string_view{data, size};
}

void
client_change_callback(RedisModuleCtx *ctx, RedisModuleEvent ev, uint64_t sub, void *data)
{
    REDISMODULE_NOT_USED(ev);

    if (sub != REDISMODULE_SUBEVENT_CLIENT_CHANGE_DISCONNECTED) {
        return;
    }

    RedisModule_AutoMemory(ctx);
    auto *ci = static_cast<RedisModuleClientInfo*>(data);

    for (const auto& key_name: association_table->remove_client(ci->id)) {
        auto* key = RedisModule_CreateString(ctx, key_name.data(), key_name.size());
        auto* key_handle = static_cast<RedisModuleKey*>(RedisModule_OpenKey(ctx, key, REDISMODULE_WRITE));

        if (RedisModule_KeyType(key_handle) == REDISMODULE_KEYTYPE_EMPTY) {
            continue;
        }

        RedisModule_DeleteKey(key_handle);
    }
}

int
key_change_callback(RedisModuleCtx *ctx, int type, const char *event, RedisModuleString *key)
{
    REDISMODULE_NOT_USED(ctx);

    if ((type == REDISMODULE_NOTIFY_GENERIC) and (strcmp(event, "del") != 0)) {
        return  REDISMODULE_OK;
    }

    association_table->remove_key(as_str_view(key));

    return REDISMODULE_OK;
}

int
ax_associate_key(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    RedisModule_AutoMemory(ctx);
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    if (RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ) == NULL) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    association_table->add(RedisModule_GetClientId(ctx), as_str_view(argv[1]));
    return RedisModule_ReplyWithLongLong(ctx, 1);
}

}

extern "C" int
RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    int ret_code;

    ret_code = RedisModule_Init(
        ctx,
        "ax_redis_auto_del",
        1,
        REDISMODULE_APIVER_1);

    if (ret_code == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    ret_code = RedisModule_SubscribeToServerEvent(
        ctx,
        RedisModuleEvent_ClientChange,
        client_change_callback);

    if (ret_code == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    ret_code = RedisModule_CreateCommand(
        ctx,
        "ax.associate_key",
        ax_associate_key,
        "readonly fast",
        1, 1, 1);

    if (ret_code == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    ret_code = RedisModule_SubscribeToKeyspaceEvents(
        ctx,
        REDISMODULE_NOTIFY_EVICTED |
        REDISMODULE_NOTIFY_EXPIRED |
        REDISMODULE_NOTIFY_GENERIC,
        key_change_callback);

    if (ret_code == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    association_table = new AssociationTable{};

    return REDISMODULE_OK;
}

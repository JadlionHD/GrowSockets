#include <enet/enet.h>
#include <napi.h>
#include <unordered_map>

#define ARG Napi::CallbackInfo& info

std::unordered_map<uint32_t, ENetPeer*> peers;
ENetHost* host;
Napi::FunctionReference emitter;

uint32_t netID = 0;

void __init(ARG) {
  Napi::Env env = info.Env();
  uint32_t port = info[0].ToNumber().Uint32Value();

  if (enet_initialize() != 0)
    return Napi::Error::New(env, "ENet failed to Initialize.").ThrowAsJavaScriptException();

  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = static_cast<uint16_t>(port);

  host = enet_host_create(&address, 1024, 2, 0, 0);
  host->checksum = enet_crc32;
  enet_host_compress_with_range_coder(host);
}

void __send(ARG) {
  Napi::Env env = info.Env();

  uint32_t peerID  = info[0].ToNumber().Uint32Value();
  uint32_t count   = info[1].ToNumber().Uint32Value();
  Napi::Object arr = info[2].ToObject();

  auto peer = peers[peerID];
  if (!peer || peer->state != ENET_PEER_STATE_CONNECTED) return;

  for (uint32_t i = 0; i < count; ++i) {
    auto buffer = arr.Get(i).As<Napi::Buffer<unsigned char>>();
    auto packet = enet_packet_create(buffer.Data(),
                                    buffer.Length(),
                                    ENET_PACKET_FLAG_RELIABLE);

    enet_peer_send(peer, 0, packet);
  }
}

void __accept(ARG) {
  Napi::Env env = info.Env();

  if (!emitter)
    return Napi::Error::New(env, "Can't watch for events without the emit function.").ThrowAsJavaScriptException();

  ENetEvent event;

  if (enet_host_service(host, &event, 0) > 0)
    switch (event.type) {
      uint32_t lastNetID = netID++;
      event.peer->data   = new uint8_t[sizeof(uint32_t)];

      *(uint32_t*)event.peer->data = lastNetID;

      case ENET_EVENT_TYPE_CONNECT: {
        emitter.Call({
          Napi::String::New(env, "connect"),
          Napi::Number::New(env, lastNetID)
        });
        break;
      }

      case ENET_EVENT_TYPE_RECEIVE: {
        auto packet = new unsigned char[event.packet->dataLength];
        memcpy(packet, event.packet->data, event.packet->dataLength);

        emitter.Call({
          Napi::String::New(env, "data"),
          Napi::Number::New(env, *(int*)event.peer->data),
          Napi::Buffer<uint8_t>::New(env,
                                    packet,
                                    event.packet->dataLength,
                                    [](Napi::Env env, uint8_t* data)
          {
            delete[] data;
          })
        });

        enet_packet_destroy(event.packet);
        break;
      }

      case ENET_EVENT_TYPE_DISCONNECT: {
        uint32_t userNetID = *(uint32_t*)event.peer->data;
        
        emitter.Call({
          Napi::String::New(env, "disconnect"),
          Napi::Number::New(env, userNetID)
        });

        if (peers[userNetID])
          peers.erase(userNetID);

        delete[] event.peer->data;

        break;
      }
    }
}

void __set_netID(ARG) {
  netID = info[0].ToNumber().Uint32Value();
}

void __close(ARG) {
  enet_deinitialize();
}

void __set_emitter(ARG) {
  emitter = Napi::Persistent(info[0].As<Napi::Function>());
}

Napi::Object __reg(Napi::Env env, Napi::Object exports) {
  exports["init"]     = Napi::Function::New(env, __init);
  exports["send"]     = Napi::Function::New(env, __send);
  exports["accept"]   = Napi::Function::New(env, __accept);
  exports["setNetID"] = Napi::Function::New(env, __set_netID);
  exports["deInit"]   = Napi::Function::New(env, __close);
  exports["emitter"]  = Napi::Function::New(env, __set_emitter);

  return exports;
}

NODE_API_MODULE(GrowSockets, __reg)
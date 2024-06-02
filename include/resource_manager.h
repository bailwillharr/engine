#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <typeinfo>

#include "log.h"

#include "resource_mesh.h"

namespace engine {

class IResourceManager {
   public:
    virtual ~IResourceManager(){};
};

template <class T>
class ResourceManager : public IResourceManager {
   public:
    ~ResourceManager() override { LOG_DEBUG("Destroying resource manager... '{}'", typeid(T).name()); }

    std::shared_ptr<T> Add(const std::string& name, std::unique_ptr<T>&& resource)
    {
        if (resources_.contains(name) == false) {
            std::shared_ptr<T> resource_shared(std::move(resource));
            resources_.emplace(name, resource_shared);
            return resource_shared;
        }
        else {
            throw std::runtime_error("Cannot add a resource which already exists");
        }
    }

    void AddPersistent(const std::string& name, std::unique_ptr<T>&& resource) { persistent_resources_.push_back(Add(name, std::move(resource))); }

    std::shared_ptr<T> Get(const std::string& name)
    {
        if (resources_.contains(name)) {
            std::weak_ptr<T> ptr = resources_.at(name);
            if (ptr.expired() == false) {
                return ptr.lock();
            }
            else {
                resources_.erase(name);
            }
        }
        // resource doesn't exist:
        throw std::runtime_error("Resource doesn't exist: " + name);
        return {};
    }

   private:
    std::unordered_map<std::string, std::weak_ptr<T>> resources_{};
    // This array owns persistent resources
    std::vector<std::shared_ptr<T>> persistent_resources_{};
};

} // namespace engine
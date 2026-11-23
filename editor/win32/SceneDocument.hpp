#pragma once
#include <array>
#include <string>
#include <vector>

class SceneDocument final {
public:
    struct Object {
        std::wstring id, name, type;
        std::array<float,3> position{0,0,0};
        std::array<float,3> rotation{0,0,0};
        std::array<float,3> scale{1,1,1};
        std::wstring mesh;
        std::wstring baseColor=L"#b8c8df";
        std::wstring texture;
        float metallic=0.0f;
        float roughness=0.5f;
    };
    SceneDocument();
    void reset();
    bool load(const std::wstring& path, std::wstring* error=nullptr);
    bool save(const std::wstring& path, std::wstring* error=nullptr);
    const std::vector<Object>& objects() const noexcept { return objects_; }
    Object* find(const std::wstring& id) noexcept;
    const Object* find(const std::wstring& id) const noexcept;
    std::wstring addObject(const std::wstring& name,const std::wstring& type);
    bool removeObject(const std::wstring& id);
    bool setObjectName(const std::wstring& id,const std::wstring& name);
    bool setTransformComponent(const std::wstring& id,int component,double value);
    const std::wstring& filePath() const noexcept { return filePath_; }
    bool isDirty() const noexcept { return dirty_; }
private:
    std::vector<Object> objects_;
    std::wstring filePath_;
    bool dirty_=false;
};

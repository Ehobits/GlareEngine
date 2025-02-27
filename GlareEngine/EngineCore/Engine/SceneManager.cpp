#include "SceneManager.h"
#include "RenderObject.h"
#include "ModelLoader.h"

using namespace  GlareEngine;


/////Scene Manager///////////////////////////


Scene* SceneManager::CreateScene(std::string name)
{
    auto scene = std::make_unique<Scene>(name, m_pCommandList);
	mScenes[name] = std::move(scene);
    return mScenes[name].get();
}

Scene* SceneManager::GetScene(std::string Name)
{
    if (mScenes.find(Name) == mScenes.end())
    {
        CreateScene(Name);
    }
    return mScenes[Name].get();
}

void SceneManager::ReleaseScene(std::string name)
{
    if (mScenes.find(name) != mScenes.end())
    {
        mScenes[name] = nullptr;
        mScenes.erase(name);
    }

}

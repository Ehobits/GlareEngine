#pragma once

namespace GlareEngine
{
	namespace GameCore
	{

		class GameApp
		{
		public:
			GameApp();

			virtual ~GameApp() {}

			//�˹��ܿ����ڳ�ʼ��Ӧ�ó���״̬�������ڷ����Ҫ��Ӳ����Դ�����С� 
			//����������Щ��Դ��ĳЩ״̬��Ӧ�ڹ��캯���г�ʼ��������ָ��ͱ�־��
			virtual void Startup(void) = 0;
			virtual void Cleanup(void) = 0;

			//ȷ�����Ƿ�Ҫ�˳���Ӧ�á� Ĭ������£�Ӧ�ó���������У�ֱ�����¡� ESC����Ϊֹ��
			virtual bool IsDone(void);

			//ÿ֡������һ��update������ ״̬���ºͳ�����Ⱦ��Ӧʹ�ô˷�������
			virtual void Update(float deltaT) = 0;

			//rendering pass
			virtual void RenderScene(void) = 0;

			//��ѡ��UI�����ǣ���Ⱦ�׶Ρ� ����LDR�� �������ѱ������ 
			virtual void RenderUI() = 0;

			//Resize
			virtual void OnResize(uint32_t width, uint32_t height) = 0;

			//Msg 
			virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

			static GameApp* GetApp() { return mGameApp; }

			float AspectRatio() const;
		
		protected:
			static GameApp* mGameApp;

			uint32_t mClientWidth;
			uint32_t mClientHeight;
		};

		void RunApplication(GameApp& app, const wchar_t* className,HINSTANCE HAND);


	}
	

#define MAIN_FUNCTION()  int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,\
	PSTR cmdLine, int showCmd)
#define CREATE_APPLICATION( app_class ) \
    MAIN_FUNCTION() \
    { \
        GameApp* app = new app_class(); \
        GlareEngine::GameCore::RunApplication( *app, L#app_class,hInstance); \
        delete app; \
        return 0; \
    }

}



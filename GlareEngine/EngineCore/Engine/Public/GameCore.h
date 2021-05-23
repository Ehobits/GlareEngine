#pragma once
namespace GlareEngine
{
	namespace GameCore
	{
		class GameApp
		{
		public:
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
			virtual void RenderUI(class GraphicsContext&) {};
		};

		void RunApplication(GameApp& app, const wchar_t* className);

	}


#define MAIN_FUNCTION()  int wmain(/*int argc, wchar_t** argv*/)
#define CREATE_APPLICATION( app_class ) \
    MAIN_FUNCTION() \
    { \
        GameApp* app = new app_class(); \
        GameCore::RunApplication( *app, L#app_class ); \
        delete app; \
        return 0; \
    }
}


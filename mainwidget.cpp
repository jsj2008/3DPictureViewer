#include "mainwidget.h"
#include <QMessageBox>
#include <QWheelEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QPalette>
#include <time.h>
#include <GL/glew.h>
#include <GL/wglext.h>
#include <Windows.h>

HGLRC _dummy_glctx;
HGLRC _real_glctx;

const int MSAA = 2;
const int WIDTH = 1280;
const int HEIGHT = 720;
const QColor FileNameColor(59, 59, 59, 255);
const QColor ResolutionColor(112, 112, 112, 255);

MainWidget::MainWidget(int fps, QWidget *parent) : fps(fps), QWidget(parent)
{
	ui.setupUi(this);

	srand((unsigned int)time(NULL));//初始化随机数种子

	initWidgetProp();
	initOpenGLContext();
	initMultiSample();
	initGlew();
	initGLStates();
	initLabels();

	//设置绘制计时器
	connect(&drawTimer, SIGNAL(timeout()), this, SLOT(update()));
	drawTimer.start(1000 / fps);

	fpsTime = new QTime;
	fpsTime->start();

	isMousePress = false;

	pictureWidget = new PictureWidget;
	editPictureDialog = new EditPictureDialog;
	editPictureDialog->hide();
	helpDialog = new QDialog;
	QPalette palette = helpDialog->palette();
	QPixmap logo("Resources/bg/logo.jpg");
	palette.setBrush(QPalette::Window, logo);
	helpDialog->setWindowTitle(QString::fromLocal8Bit("关于"));
	helpDialog->setPalette(palette);
	helpDialog->resize(logo.size());
	helpDialog->hide();

	Scene::initSingletons(WIDTH, HEIGHT);
	scene = new Scene(WIDTH, HEIGHT);

	connect(scene, SIGNAL(setFileName(QString)), this, SLOT(setFileName(QString)));
	connect(scene, SIGNAL(setResolution(int, int)), this, SLOT(setResolution(int, int)));
	connect(scene, SIGNAL(setAlpha(float)), this, SLOT(setAlpha(float)));
	connect(scene, SIGNAL(displayCenterPicture(QString)), this, SLOT(displayCenterPicture(QString)));
	connect(scene, SIGNAL(showEditPictureDialog(QString, QString, QString, int, int)), this, SLOT(showEditPictureDialog(QString, QString, QString, int, int)));
	connect(scene, SIGNAL(showHelpDialog()), this, SLOT(showHelpDialog()));
	connect(pictureWidget, SIGNAL(closing()), this, SLOT(show()));
	connect(editPictureDialog, SIGNAL(reloadPicture(QString)), scene, SLOT(reloadPicture(QString)));
}

MainWidget::~MainWidget()
{
	Scene::destorySingletons();

	delete fpsTime;
	delete scene;
	delete fileNameLabel;
	delete resolutionLabel;
	delete pictureWidget;
	delete editPictureDialog;
	delete helpDialog;

	wglDeleteContext(_dummy_glctx);
	wglDeleteContext(_real_glctx);
}

void MainWidget::logic(float deltaTime)
{
	int deltaMousePosX = 0;
	curMousePos = mapFromGlobal(cursor().pos());
	if (isMousePress)
	{
		deltaMousePosX = curMousePos.x() - prevMousePos.x();
		prevMousePos = mapFromGlobal(cursor().pos());
	}
	scene->mouseMove(curMousePos.x(), curMousePos.y());
	scene->logic(deltaTime, deltaMousePosX);
}

void MainWidget::render()
{
	scene->render();
}

void MainWidget::paintEvent(QPaintEvent* evt)
{
	tick();

	logic(deltaTime);
	render();
	renderLabels();

	HWND hwnd = (HWND)winId();
	HDC hdc = GetDC(hwnd);
	SwapBuffers(hdc);
	ReleaseDC(hwnd, hdc);
}

void MainWidget::renderLabels()
{
	fileNameLabel->move(pos() + QPoint((width() - fileNameLabel->width()) / 2, 40));
	resolutionLabel->move(pos() + QPoint((width() - resolutionLabel->width()) / 2, 70));
}

void MainWidget::keyPressEvent(QKeyEvent *evt)
{
	switch (evt->key())
	{
	case Qt::Key_Escape:
		close();
		break;
	case Qt::Key_Left:
		scene->prevBtnClicked();
		break;
	case Qt::Key_Right:
		scene->nextBtnClicked();
		break;
	}
}

void MainWidget::mousePressEvent(QMouseEvent *evt)
{
	isMousePress = true;
	prevMousePos = evt->pos();
	curMousePos = evt->pos();

	scene->mousePress(evt->pos().x(), evt->pos().y());
}

void MainWidget::mouseReleaseEvent(QMouseEvent *evt)
{
	isMousePress = false;
	scene->addEaseOutAction();
	scene->mouseRelease(evt->pos().x(), evt->pos().y());
}

void MainWidget::mouseDoubleClickEvent(QMouseEvent *evt)
{
	QString centerPicturePath = scene->getCenterPicturePath(evt->pos());
	if (!centerPicturePath.isNull())
	{
		QPixmap centerPicture(centerPicturePath);
		pictureWidget->setPicturePath(centerPicturePath);
		pictureWidget->showMaximized();
		close();
	}
}

void MainWidget::dragEnterEvent(QDragEnterEvent *evt)
{
	if (evt->mimeData()->hasUrls())
	{
		QList<QUrl> urls = evt->mimeData()->urls();
		QString str = urls.first().toLocalFile();
		QFileInfo fi(str);

		if (fi.isDir())
		{
			evt->acceptProposedAction();
		}
	}
}

void MainWidget::dropEvent(QDropEvent *evt)
{
	if (evt->mimeData()->hasUrls()) 
	{
		QList<QUrl> urls = evt->mimeData()->urls();
		QString str = urls.first().toLocalFile();
		QFileInfo fi(str);

		if (fi.isDir())
		{
			scene->load(str);
		}
	}
}

void MainWidget::closeEvent(QCloseEvent *evt)
{
	fileNameLabel->close();
	resolutionLabel->close();
}

void MainWidget::changeEvent(QEvent *evt)
{
	if (evt->type() == QEvent::WindowStateChange && windowState() & Qt::WindowMinimized)
	{
		fileNameLabel->showMinimized();
		resolutionLabel->showMinimized();
	}
	else if (evt->type() == QEvent::ActivationChange & isActiveWindow())
	{
		fileNameLabel->showNormal();
		resolutionLabel->showNormal();
	}
}

void MainWidget::setFileName(QString fileName)
{
	fileNameLabel->setText(QString::fromLocal8Bit("文件名：") + fileName);
	fileNameLabel->adjustSize();
}

void MainWidget::setResolution(int width, int height)
{
	resolutionLabel->setText(QString::fromLocal8Bit("分辨率：%1, %2").arg(width).arg(height));
	resolutionLabel->adjustSize();
}

void MainWidget::setAlpha(float alpha)
{
	QPalette p1;
	p1.setColor(QPalette::WindowText, QColor(FileNameColor.red(), FileNameColor.green(), FileNameColor.blue(), alpha * 255));
	fileNameLabel->setPalette(p1);

	QPalette p2;
	p2.setColor(QPalette::WindowText, QColor(ResolutionColor.red(), ResolutionColor.green(), ResolutionColor.blue(), alpha * 255));
	resolutionLabel->setPalette(p2);
}

void MainWidget::displayCenterPicture(QString centerPicturePath)
{

	QPixmap centerPicture(centerPicturePath);
	pictureWidget->setPicturePath(centerPicturePath);
	pictureWidget->showMaximized();
	close();
}

void MainWidget::showEditPictureDialog(QString path, QString fileBaseName, QString fileSuffix, int width, int height)
{
	editPictureDialog->set(path, fileBaseName, fileSuffix, width, height);
	editPictureDialog->exec();
}

void MainWidget::showHelpDialog()
{
	helpDialog->exec();
}

void MainWidget::initWidgetProp()//初始化widget的一些属性
{
	setWindowOpacity(1);
	setWindowTitle(QString::fromLocal8Bit("3D图片浏览器"));

	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_PaintOnScreen);

	setAcceptDrops(true);

	setFixedSize(WIDTH, HEIGHT);
}

void MainWidget::initOpenGLContext()//初始化window界面context
{
	QMessageBox *box = new QMessageBox;
	HWND hwnd = (HWND)box->winId();
	HDC hdc = GetDC(hwnd);
	//[METHOD] SetWindowPixelFormat(HDC)

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,                        // nVersion
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER
		| PFD_SUPPORT_GDI/*|PFD_STEREO*/,    // dwFlags
		PFD_TYPE_RGBA,            // PixelType
		24,                       // ColorBits
		0, 0, 0, 0, 0, 0,         // RGB bits and shifts
		8,                        // cAlphaBits
		0,                        // cAlphaShift
		0, 0, 0, 0, 0,            // Accum
		24,                       // Depth
		8,                        // Stencil
		0,                        // cAuxBuffers
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	int pixelFormat;
	pixelFormat = ChoosePixelFormat(hdc, &pfd);
	if (pixelFormat == 0) {
		pixelFormat = 1;
		if (DescribePixelFormat(hdc, pixelFormat, sizeof(pfd), &pfd) == 0)
			return;
	}

	if (SetPixelFormat(hdc, pixelFormat, &pfd) == FALSE) return;

	//[NOTE] !DO NOT! change check order
	if (!(_dummy_glctx = wglCreateContext(hdc)) || !wglMakeCurrent(hdc, _dummy_glctx))
		return;
}

void MainWidget::initGlew()//初始化Glew
{
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		printf("Failed to initialize GLEW\n");
		exit(-1);
	}
}

void MainWidget::initGLStates()//初始化opengl参数
{
	glClearColor(0, 1, 0, 1);

	//glFrontFace(GL_CCW);
	//glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}

void MainWidget::initLabels()
{
	fileNameLabel = new QLabel(this);
	fileNameLabel->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
	fileNameLabel->setAttribute(Qt::WA_TranslucentBackground);
	fileNameLabel->setFont(QFont("Microsoft YaHei", 18));
	QPalette p1;
	p1.setColor(QPalette::WindowText, FileNameColor);
	fileNameLabel->setPalette(p1);
	fileNameLabel->adjustSize();
	fileNameLabel->show();

	resolutionLabel = new QLabel(this);
	resolutionLabel->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
	resolutionLabel->setAttribute(Qt::WA_TranslucentBackground);
	resolutionLabel->setFont(QFont("Microsoft YaHei", 14));
	QPalette p2;
	p2.setColor(QPalette::WindowText, ResolutionColor);
	resolutionLabel->setPalette(p2);
	resolutionLabel->adjustSize();
	resolutionLabel->show();
}

bool MainWidget::initMultiSample()//设置MultiSample
{
	HWND hwnd = (HWND)winId();
	HDC hdc = GetDC(hwnd);
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,                        // nVersion
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER
		| PFD_SUPPORT_GDI/*|PFD_STEREO*/,    // dwFlags
		PFD_TYPE_RGBA,            // PixelType
		24,                       // ColorBits
		0, 0, 0, 0, 0, 0,         // RGB bits and shifts
		8,                        // cAlphaBits
		0,                        // cAlphaShift
		0, 0, 0, 0, 0,            // Accum
		24,                       // Depth
		8,                        // Stencil
		0,                        // cAuxBuffers
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};
	int pixelFormat;
	if (MSAA == -1)
	{
		pixelFormat = ChoosePixelFormat(hdc, &pfd);
		if (pixelFormat == 0) {
			pixelFormat = 1;
			if (DescribePixelFormat(hdc, pixelFormat, sizeof(pfd), &pfd) == 0)
				return false;
		}
	}
	else if (!setMultisample(hwnd, MSAA, pixelFormat))
	{
		return false;
	}
	if (SetPixelFormat(hdc, pixelFormat, &pfd) == FALSE)
		return false;

	//[NOTE] !DO NOT! change check order
	if (!(_real_glctx = wglCreateContext(hdc)) || !wglMakeCurrent(hdc, _real_glctx))
		return false;

	if (MSAA != -1)
	{
		glEnable(GL_MULTISAMPLE_ARB);
	}

	return true;
}

bool MainWidget::setMultisample(HWND hWnd, int sampleNum, int &pixelFormat)
{
	// See If The String Exists In WGL!
	if (!wglIsExtensionSupported("WGL_ARB_multisample"))
	{
		return false;
	}

	// Get Our Pixel Format
	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
	if (!wglChoosePixelFormatARB)
	{
		return false;
	}

	// Get Our Current Device Context
	HDC hDC = GetDC(hWnd);

	int		valid;
	UINT	numFormats;
	float	fAttributes[] = { 0, 0 };

	// These Attributes Are The Bits We Want To Test For In Our Sample
	// Everything Is Pretty Standard, The Only One We Want To 
	// Really Focus On Is The SAMPLE BUFFERS ARB And WGL SAMPLES
	// These Two Are Going To Do The Main Testing For Whether Or Not
	// We Support Multisampling On This Hardware.
	int iAttributes[] =
	{
		WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
		WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
		WGL_COLOR_BITS_ARB, 24,
		WGL_ALPHA_BITS_ARB, 8,
		WGL_DEPTH_BITS_ARB, 16,
		WGL_STENCIL_BITS_ARB, 0,
		WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
		WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
		WGL_SAMPLES_ARB, sampleNum,
		0, 0
	};

	// First We Check To See If We Can Get A Pixel Format For 4 Samples
	valid = wglChoosePixelFormatARB(hDC, iAttributes, fAttributes, 1, &pixelFormat, &numFormats);

	// If We Returned True, And Our Format Count Is Greater Than 1
	if (valid && numFormats >= 1)
	{
		return true;
	}
	return false;
}

bool MainWidget::wglIsExtensionSupported(const char *extension)
{
	const size_t extlen = strlen(extension);
	const char *supported = NULL;

	// Try To Use wglGetExtensionStringARB On Current DC, If Possible
	PROC wglGetExtString = wglGetProcAddress("wglGetExtensionsStringARB");

	if (wglGetExtString)
		supported = ((char*(__stdcall*)(HDC))wglGetExtString)(wglGetCurrentDC());

	// If That Failed, Try Standard Opengl Extensions String
	if (supported == NULL)
		supported = (char*)glGetString(GL_EXTENSIONS);

	// If That Failed Too, Must Be No Extensions Supported
	if (supported == NULL)
		return false;

	// Begin Examination At Start Of String, Increment By 1 On False Match
	for (const char* p = supported;; p++)
	{
		// Advance p Up To The Next Possible Match
		p = strstr(p, extension);

		if (p == NULL)
			return false;															// No Match

		// Make Sure That Match Is At The Start Of The String Or That
		// The Previous Char Is A Space, Or Else We Could Accidentally
		// Match "wglFunkywglExtension" With "wglExtension"

		// Also, Make Sure That The Following Character Is Space Or NULL
		// Or Else "wglExtensionTwo" Might Match "wglExtension"
		if ((p == supported || p[-1] == ' ') && (p[extlen] == '\0' || p[extlen] == ' '))
			return true;															// Match
	}
}

void MainWidget::tick()
{
	deltaTime = fpsTime->elapsed();
	deltaTime /= 1000.0f;
	fpsTime->restart();

	if (deltaTime > 2.0 / fps)
	{
		deltaTime = 1.0 / fps;
	}
}
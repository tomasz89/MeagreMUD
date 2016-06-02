#include <QtWidgets>

namespace MeagreMud
{

class MeagreMud : public QMainWindow
{
public:
    MeagreMud();
    ~MeagreMud();
public Q_SLOTS:
    void save();
private:
    void createMenus();
    void createTools();

    QMenu *fileMenu;
};

}

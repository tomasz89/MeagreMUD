#include <QtWidgets>

#include "configpages.h"

MeagreMud::BbsPage::BbsPage(QWidget *parent)
    : QWidget(parent)
{
    QGroupBox *configGroup = new QGroupBox(tr("Connection"));

    QLabel *serverLabel = new QLabel(tr("Server:"));
    QLineEdit *serverAddress = new QLineEdit();
    QLabel *portLabel = new QLabel(tr("Port"));
    QSpinBox *port = new QSpinBox();
    port->setMaximum(65535);
    port->setMinimum(1);
    port->setValue(23);

    QHBoxLayout *serverLayout = new QHBoxLayout;
    serverLayout->addWidget(serverLabel);
    serverLayout->addWidget(serverAddress);
    serverLayout->addWidget(portLabel);
    serverLayout->addWidget(port);

    QVBoxLayout *configLayout = new QVBoxLayout;
    configLayout->addLayout(serverLayout);
    configGroup->setLayout(configLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(configGroup);
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}

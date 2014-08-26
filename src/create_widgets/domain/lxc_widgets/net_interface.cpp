#include "net_interface.h"

LXC_NetInterface::LXC_NetInterface(QWidget *parent, QStringList nets) :
    _QWidget(parent), existNetwork(nets)
{
    setObjectName("Network:Device");
    useExistNetwork = new QCheckBox("Use Exist Network", this);
    networks = new QComboBox(this);
    networks->addItems( nets );

    bridgeName = new QLineEdit(this);
    bridgeName->setPlaceholderText("Enter Exist Virtual Bridge Name");
    mac = new QLineEdit(this);
    mac->setPlaceholderText("00:11:22:33:44:55 (optional)");
    macLabel = new QLabel("MAC:", this);
    netDescLayout = new QGridLayout();
    netDescLayout->addWidget(bridgeName, 0, 0, 1, 2);
    netDescLayout->addWidget(macLabel, 2, 0, Qt::AlignLeft);
    netDescLayout->addWidget(mac, 2, 1);
    netDescWdg = new QWidget(this);
    netDescWdg->setLayout(netDescLayout);
    commonLayout = new QVBoxLayout();
    commonLayout->addWidget(useExistNetwork);
    commonLayout->addWidget(networks);
    commonLayout->addWidget(netDescWdg);
    commonLayout->insertStretch(-1);
    setLayout(commonLayout);
    connect(useExistNetwork, SIGNAL(toggled(bool)), this, SLOT(changeUsedNetwork(bool)));
    connect(networks, SIGNAL(currentIndexChanged(QString)), this, SLOT(changeUsedNetwork(QString)));
    useExistNetwork->setChecked(true);
}
LXC_NetInterface::~LXC_NetInterface()
{
    disconnect(useExistNetwork, SIGNAL(toggled(bool)), this, SLOT(changeUsedNetwork(bool)));
    disconnect(networks, SIGNAL(currentIndexChanged(QString)), this, SLOT(changeUsedNetwork(QString)));
    delete useExistNetwork;
    useExistNetwork = 0;
    delete networks;
    networks = 0;

    delete bridgeName;
    bridgeName = 0;
    delete macLabel;
    macLabel = 0;
    delete mac;
    mac = 0;
    delete netDescLayout;
    delete netDescWdg;
    netDescWdg = 0;
    delete commonLayout;
    commonLayout = 0;
}

/* public slots */
QDomNodeList LXC_NetInterface::getNodeList() const
{
    QDomDocument doc = QDomDocument();
    QDomElement _source, _mac;
    _source= doc.createElement("source");
    if ( !useExistNetwork->isChecked() ) {
        _source.setAttribute("bridge", bridgeName->text());
        doc.appendChild(_source);

        if ( !mac->text().isEmpty() ) {
            _mac= doc.createElement("mac");
            _mac.setAttribute("address", mac->text());
            doc.appendChild(_mac);
        };
    } else {
        _source.setAttribute("network", networks->currentText());
        doc.appendChild(_source);
    };

    //qDebug()<<doc.toString();
    return doc.childNodes();
}
QString LXC_NetInterface::getDevType() const
{
    return (useExistNetwork->isChecked())? "network" : "bridge";
}

/* private slots */
void LXC_NetInterface::changeUsedNetwork(bool state)
{
    if (state) {
        networks->setEnabled(true);
        netDescWdg->setEnabled(false);
    } else  {
        networks->setEnabled(false);
        netDescWdg->setEnabled(true);
    };
}
void LXC_NetInterface::changeUsedNetwork(QString item)
{
    qDebug()<<item;
}
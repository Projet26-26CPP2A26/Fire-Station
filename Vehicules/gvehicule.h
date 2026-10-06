#ifndef GVEHICULE_H
#define GVEHICULE_H

#include <QColor>
#include <QList>
#include <QMainWindow>

#include "personnel.h"
#include "statsdialog.h"
#include "vehicule.h"

class QLabel;
class QTableWidget;
class QTableWidgetItem;

QT_BEGIN_NAMESPACE
namespace Ui {
class GVehicule;
}
QT_END_NAMESPACE

// Projet individuel : module "Gestion des véhicules".
// Les noms des objets et des slots sont les mêmes que dans le projet d'intégration
// SmartFireStation : l'intégration se fait donc par simple copier-coller.
class GVehicule : public QMainWindow
{
    Q_OBJECT

public:
    GVehicule(QWidget *parent = 0);
    ~GVehicule();

private slots:
    void on_btnQuitterVehicule_clicked();

    // ----- Module Véhicules (slots créés automatiquement par le nom on_<objet>_<signal>) -----
    void on_btnAjouterVehicule_clicked();
    void on_btnModifierVehicule_clicked();
    void on_btnSupprimerVehicule_clicked();
    void on_btnRechercherVehicule_clicked();
    void on_lineEditRechercheVehicule_textChanged(const QString &texte);
    void on_btnTrierVehicule_clicked();
    void on_btnExportVehicule_clicked();
    void on_btnStatsVehicule_clicked();
    void on_tableWidgetVehicules_cellDoubleClicked(int ligne, int colonne);
    void on_comboIntervention_currentIndexChanged(int index);
    void on_btnUtiliserVehicule_clicked();

private:
    Ui::GVehicule *ui;

    // ----- Module Véhicules -----
    void setupVehicules();
    void chargerVehiculesExemple();
    void refreshVehiculesTable();
    void refreshVehiculePropose();
    void filtrerVehicules();
    int vehiculeSelectionne();
    QString prochainIdVehicule();
    bool doitEchangerVehicules(Vehicule a, Vehicule b, int critere);
    StatCritere statVehiculesParEtat();
    StatCritere statVehiculesParDispo();
    QList<Vehicule> m_vehicules;
    int m_vehiculePropose; // numéro du véhicule proposé pour la mission (-1 = aucun)

    // ----- Outils communs -----
    QTableWidgetItem *creerItem(const QString &texte, const QColor &couleur, bool gras);
    void setLabelClass(QLabel *label, const QString &classe);
    void exporterTableauPdf(QTableWidget *table, const QString &titre, const QString &nomFichier);
};

#endif // GVEHICULE_H

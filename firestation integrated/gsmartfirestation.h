#pragma once
// ===== databasemanager =====

#include <QSqlDatabase>
#include <QWidget>

class DatabaseManager
{
public:
    static bool connectInteractive(QWidget *parent);
    static bool isConnected();
    static QSqlDatabase database();
    static void close();
private:
    static QSqlDatabase s_db;
    static bool s_connected;
};

// ===== styleutils =====

#include <QList>
#include <QStyle>
#include <QWidget>

// Les propriétés class="..." (ex : class="btnPrimary") ne sont prises en compte par style.qss
// qu'après un "re-polish" : on demande au style de se réappliquer sur chaque widget.
inline void repolish(QWidget *fenetre)
{
    QList<QWidget *> widgets = fenetre->findChildren<QWidget *>();
    for (int i = 0; i < widgets.size(); i++) {
        widgets[i]->style()->unpolish(widgets[i]);
        widgets[i]->style()->polish(widgets[i]);
    }
    fenetre->style()->unpolish(fenetre);
    fenetre->style()->polish(fenetre);
}


// ===== pdfexport =====

#include <QList>
#include <QString>
#include <QStringList>

// Crée un fichier PDF contenant un titre et un tableau. Retourne true si le fichier est créé.
bool creerPdfTableau(const QString &chemin, const QString &titre,
                     const QStringList &entetes, const QList<QStringList> &lignes);


// ===== vehicule =====

#include <QDate>
#include <QString>
#include <QStringList>

// Entité "Véhicule" (Module 2 : Gestion des véhicules)
class Vehicule
{
public:
    Vehicule();
    Vehicule(const QString &id, const QString &immatriculation, const QString &type,
             const QString &modele, const QString &etat, const QString &disponibilite,
             const QDate &dateMaintenance);

    // Getters
    QString id() const { return m_id; }
    QString immatriculation() const { return m_immatriculation; }
    QString type() const { return m_type; }
    QString modele() const { return m_modele; }
    QString etat() const { return m_etat; }
    QString disponibilite() const { return m_disponibilite; }
    QDate dateMaintenance() const { return m_dateMaintenance; }

    // Setters
    void setId(const QString &v) { m_id = v; }
    void setImmatriculation(const QString &v) { m_immatriculation = v; }
    void setType(const QString &v) { m_type = v; }
    void setModele(const QString &v) { m_modele = v; }
    void setEtat(const QString &v) { m_etat = v; }
    void setDisponibilite(const QString &v) { m_disponibilite = v; }
    void setDateMaintenance(const QDate &v) { m_dateMaintenance = v; }

    // Vrai si la maintenance est prévue dans les 30 prochains jours (ou déjà dépassée)
    bool maintenanceProche() const;

    // Listes de valeurs autorisées
    static QStringList types();
    static QStringList etats();
    static QStringList disponibilites();
    static QStringList typesPourIntervention(const QString &intervention);

private:
    QString m_id;
    QString m_immatriculation;
    QString m_type;
    QString m_modele;
    QString m_etat;
    QString m_disponibilite;
    QDate m_dateMaintenance;
};


// ===== piechartwidget =====
#include <QWidget>
#include <QStringList>
#include <QList>
class PieChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PieChartWidget(const QString &centerLabel, QWidget *parent = 0);
    void setData(const QStringList &labels, const QList<int> &values);
protected:
    void paintEvent(QPaintEvent *event);
private:
    QString m_centerLabel;
    QStringList m_labels;
    QList<int> m_values;
};

// ===== statsdialog =====

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

class QComboBox;
class QGridLayout;

// Un critère de statistique. Exemple : nom = "Grade",
// etiquettes = Capitaine, Sergent...  et valeurs = 1, 2...
struct StatCritere
{
    QString nom;
    QStringList etiquettes;
    QList<int> valeurs;
};

// Compte, pour chaque valeur possible, combien de fois elle apparaît dans 'reelles'
StatCritere compterParValeur(const QString &nom, const QStringList &possibles,
                             const QStringList &reelles);

// Fenêtre de statistiques graphiques avec diagramme circulaire (donut) et pourcentages
class StatsDialog : public QDialog
{
    Q_OBJECT
public:
    StatsDialog(const QString &titre, const QList<StatCritere> &criteres, QWidget *parent = 0);

private slots:
    void afficherCritere(int index);

private:
    QList<StatCritere> m_criteres;
    QComboBox *m_combo;
    QGridLayout *m_grille;
};


// ===== personnel (dashboard) =====

#include <QString>

// Ligne lue en base pour le Dashboard et la détection de surcharge.
// La charge opérationnelle reprend la règle du module Véhicules :
// chaque mission en cours = 20 %, maximum 100 %.
struct PersonnelDashboard
{
    QString id;             // "P001"
    QString nomComplet;
    QString grade;
    QString disponibilite;
    int missionsEnCours;
    int charge() const { int c = missionsEnCours * 20; return c > 100 ? 100 : c; }
};


// ===== gsmartfirestation =====

#include <QColor>
#include <QList>
#include <QMainWindow>


class QLabel;
class QProgressBar;
class QTableWidget;
class QTableWidgetItem;
class QLineEdit;
class QComboBox;
class QGroupBox;
class QWidget;
class QDateEdit;
class PieChartWidget;
class PersonnelPage;    // module Personnel intégré (personnel.h)

QT_BEGIN_NAMESPACE
namespace Ui {
class GSmartFireStation;
}
QT_END_NAMESPACE

// Fenêtre principale (G = Gestion). UN SEUL fichier .ui : les vues sont les pages
// d'un QStackedWidget (page 0 = Dashboard, 1 = Personnel, 2 = Véhicules).
//
// Intégration :
//   - le module Véhicules vient du projet Vehicules_min (inchangé) ;
//   - le module Personnel est la page de votre camarade (personnel.h / personnel.cpp),
//     insérée à l'index 1 ; sa connexion Oracle utilise DatabaseManager (partagée).
class GSmartFireStation : public QMainWindow
{
    Q_OBJECT

public:
    GSmartFireStation(QWidget *parent = 0);
    ~GSmartFireStation();

    // Connexion Oracle + authentification + chargement des données.
    // Retourne false si l'utilisateur annule l'authentification (l'application se ferme).
    bool demarrer();

private slots:
    // ----- Navigation entre les pages -----
    void on_btnNavDashboard_clicked();
    void on_btnNavPersonnel_clicked();
    void on_btnNavVehicules_clicked();
    void on_btnGererPersonnel_clicked();
    void on_btnGererVehicules_clicked();
    void on_btnRetourVehicule_clicked();
    void on_btnQuitter_clicked();
    void on_btnVoirStats_clicked();

    // ----- Détection de surcharge (Dashboard) -----
    void on_spinSeuil_valueChanged(int valeur);
    void on_btnDetailsSurcharge_clicked();

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
    void enregistrerVehiculeInline();
    void annulerVehiculeInline();

private:
    // ----- Navigation + Dashboard -----
    void goToPage(int index);
    void chargerPersonnelDashboard();   // lit personnel + missions en cours (Oracle)
    void refreshDashboard();
    void refreshSurcharge();
    void majBarre(QProgressBar *barre, QLabel *label, int valeur, int total);
    void ajouterAlerte(const QString &texte, const QString &couleur);
    StatCritere statPersonnelParGrade();
    StatCritere statPersonnelParDispo();
    QList<PersonnelDashboard> m_personnelDashboard;
    PersonnelPage *m_personnelPage;

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
    QLineEdit *m_vehicleImmat;
    QComboBox *m_vehicleType;
    QLineEdit *m_vehicleModele;
    QComboBox *m_vehicleEtat;
    QComboBox *m_vehicleDispo;
    QDateEdit *m_vehicleDate;
    QLabel *m_vehicleId;
    QGroupBox *m_formVehicule;
    int m_vehiculeEditIndex;
    QWidget *m_vehiculeStatsChart;

    // ----- Outils communs -----
    QTableWidgetItem *creerItem(const QString &texte, const QColor &couleur, bool gras);
    void setLabelClass(QLabel *label, const QString &classe);
    void exporterTableauPdf(QTableWidget *table, const QString &titre, const QString &nomFichier);

    Ui::GSmartFireStation *ui;
};




// ===================== MODULE PERSONNEL (integrated) =====================


// =====================================================================
//  SMART FIRE STATION - Module 1 : Gestion du personnel
//  UN SEUL fichier d'en-tête (.h) : personnel.h
//  UN SEUL fichier d'implémentation (.cpp) : personnel.cpp
//
//  Contenu :
//    1. Personnel        : entité + CRUD + métiers + contrôle de saisie (couche MODELE, tout le SQL)
//    2. PersonnelModel   : modèle d'affichage du tableau (couleurs, formats)
//    3. PersonnelPieChart: diagramme circulaire des statistiques
//    4. PersonnelPage    : interface (couche VUE, aucune requête SQL),
//                          page insérée dans la fenêtre principale intégrée
//
//  La connexion Oracle est fournie par DatabaseManager (gsmartfirestation.h),
//  partagée avec le module Véhicules : une seule connexion pour l'application.
// =====================================================================

#include <QList>
#include <QSqlDatabase>
#include <QSqlQueryModel>
#include <QString>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPushButton;
class QTableView;

// ---------------------------------------------------------------------
//  1. Entité Personnel
//     Toutes les requêtes SQL sont ici (jamais derrière un bouton).
//     Les requêtes sont préparées (prepare / bindValue / exec).
// ---------------------------------------------------------------------
class Personnel
{
public:
    Personnel();
    Personnel(int id, const QString &nom, const QString &prenom, const QString &grade,
              const QString &poste, const QString &telephone, const QString &email,
              const QString &motDePasse, const QString &questionSecrete,
              const QString &reponseSecrete, const QString &disponibilite);

    // ----- Getters -----
    int id() const { return m_id; }
    QString nom() const { return m_nom; }
    QString prenom() const { return m_prenom; }
    QString grade() const { return m_grade; }
    QString poste() const { return m_poste; }
    QString telephone() const { return m_telephone; }
    QString email() const { return m_email; }
    QString motDePasse() const { return m_motDePasse; }
    QString questionSecrete() const { return m_questionSecrete; }
    QString reponseSecrete() const { return m_reponseSecrete; }
    QString disponibilite() const { return m_disponibilite; }
    QString nomComplet() const { return m_prenom + " " + m_nom; }

    // ----- Setters -----
    void setId(int v) { m_id = v; }
    void setNom(const QString &v) { m_nom = v; }
    void setPrenom(const QString &v) { m_prenom = v; }
    void setGrade(const QString &v) { m_grade = v; }
    void setPoste(const QString &v) { m_poste = v; }
    void setTelephone(const QString &v) { m_telephone = v; }
    void setEmail(const QString &v) { m_email = v; }
    void setMotDePasse(const QString &v) { m_motDePasse = v; }
    void setQuestionSecrete(const QString &v) { m_questionSecrete = v; }
    void setReponseSecrete(const QString &v) { m_reponseSecrete = v; }
    void setDisponibilite(const QString &v) { m_disponibilite = v; }

    // ----- CRUD -----
    bool ajouter() const;                                   // Create
    bool modifier() const;                                  // Update (mot de passe / réponse inchangés si vides)
    static bool supprimer(int id);                          // Delete
    static QSqlQueryModel *afficher(const QString &recherche = QString(), int tri = 0); // Read
    static bool trouver(int id, Personnel &resultat);       // Lire un membre (remplir le formulaire)

    // ----- Métiers avancés : authentification et mot de passe oublié -----
    static bool authentifier(const QString &email, const QString &motDePasse, Personnel &connecte);
    static QString questionDe(const QString &email);        // question secrète du compte ("" si inconnu)
    static bool reinitialiserMotDePasse(const QString &email, const QString &reponse,
                                        const QString &nouveauMotDePasse);

    // ----- Statistiques : nombre de membres pour chaque grade (même ordre que grades()) -----
    static QList<int> statistiquesParGrade();

    // ----- Contrôle de saisie -----
    static bool nomValide(const QString &texte);
    static bool telephoneValide(const QString &texte);
    static bool emailValide(const QString &texte);
    static bool motDePasseValide(const QString &texte);     // 6 caractères min., au moins une lettre et un chiffre

    // ----- Outils -----
    static QString hacher(const QString &texte);            // SHA-256 (le mot de passe n'est jamais stocké en clair)
    static QString normaliser(const QString &reponse);      // réponse secrète : sans espaces ni majuscules
    static QString derniereErreur();                        // dernier message d'erreur SQL

    // ----- Listes de valeurs -----
    static QStringList grades();
    static QStringList postes();
    static QStringList disponibilites();
    static QStringList questions();

private:
    int m_id;
    QString m_nom;
    QString m_prenom;
    QString m_grade;
    QString m_poste;
    QString m_telephone;
    QString m_email;
    QString m_motDePasse;
    QString m_questionSecrete;
    QString m_reponseSecrete;
    QString m_disponibilite;
};

// ---------------------------------------------------------------------
//  2. Modèle d'affichage du tableau : ID "P001", téléphone "22 000 111",
//     disponibilité colorée. Les données viennent de Personnel::afficher().
// ---------------------------------------------------------------------
class PersonnelModel : public QSqlQueryModel
{
public:
    explicit PersonnelModel(QObject *parent = nullptr) : QSqlQueryModel(parent) {}
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};

// ---------------------------------------------------------------------
//  3. Diagramme circulaire (statistiques par grade)
// ---------------------------------------------------------------------
class PersonnelPieChart : public QWidget
{
    Q_OBJECT

public:
    explicit PersonnelPieChart(QWidget *parent = nullptr);
    void setData(const QStringList &labels, const QList<int> &values);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QStringList m_labels;
    QList<int> m_values;
};

// ---------------------------------------------------------------------
//  4. Page "Gestion du personnel" (couche VUE) : aucune requête SQL ici,
//     tout passe par la classe Personnel. Insérée dans le QStackedWidget
//     de GSmartFireStation (page 1).
// ---------------------------------------------------------------------
class PersonnelPage : public QWidget
{
    Q_OBJECT

public:
    explicit PersonnelPage(QWidget *parent = nullptr);
    ~PersonnelPage();

    bool connexion();                       // dialogue d'authentification (avant d'afficher la fenêtre)
    Personnel utilisateurConnecte() const { return m_utilisateur; }

public slots:
    void rafraichir();                      // recharge le tableau + les statistiques (appelé après connexion)

private slots:
    void ajouterPersonnel();                // prépare le formulaire pour un nouveau membre
    void modifierPersonnel();               // charge le membre sélectionné dans le formulaire
    void supprimerPersonnel();
    void enregistrer();                     // valide le formulaire : INSERT ou UPDATE
    void viderFormulaire();
    void rechercher();
    void trier();
    void exporterPdf();

private:
    void construireInterface();
    void motDePasseOublie(QWidget *parent);
    bool lireFormulaire(Personnel &p, bool modification);
    int idSelectionne() const;              // ID de la ligne sélectionnée (0 si aucune)
    void mettreAJourStatistiques();
    void showMessage(const QString &texte, int duree = 0);   // relaie vers la barre d'état de la fenêtre principale
    void clearMessage();

    QSqlQueryModel *m_model;
    Personnel m_utilisateur;
    int m_idEdition;                        // 0 = nouveau membre, sinon id du membre en modification
    int m_tri;                              // critère de tri appliqué (0 = ID, 1 = grade, 2 = disponibilité, 3 = nom)

    // Recherche, tri, tableau
    QLineEdit *m_recherche;
    QComboBox *m_comboTri;
    QTableView *m_table;

    // Formulaire
    QGroupBox *m_formulaire;
    QLabel *m_labelId;
    QLineEdit *m_nom;
    QLineEdit *m_prenom;
    QLineEdit *m_telephone;
    QLineEdit *m_email;
    QLineEdit *m_motDePasse;
    QLineEdit *m_reponse;
    QComboBox *m_grade;
    QComboBox *m_poste;
    QComboBox *m_disponibilite;
    QComboBox *m_question;

    // Statistiques
    PersonnelPieChart *m_graphique;
    QLabel *m_labelTotal;
};



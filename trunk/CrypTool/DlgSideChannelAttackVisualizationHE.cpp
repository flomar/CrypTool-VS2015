/**************************************************************************

  Copyright [2009] [CrypTool Team]

  This file is part of CrypTool.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

**************************************************************************/

#include "stdafx.h"
#include "CrypToolApp.h"
#include "CrypToolBase.h"

#include "DlgSideChannelAttackVisualizationHE.h"

#include "DlgHybridDecryptionDemo.h"

#include "CrypToolTools.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include "FileTools.h"
#include "CrypToolTools.h"

CDlgSideChannelAttackVisualizationHE::CDlgSideChannelAttackVisualizationHE(CWnd* pParent) :
	CDialog(CDlgSideChannelAttackVisualizationHE::IDD, pParent), scaBigNumberSettings() {
	
	m_bShowInfoDialogues = FALSE;

	// *** Initialisierungen ***
	this->initMode = 0;
	this->initFile = "";
	this->isFileDeclared = false;
	this->targetFile = "";
	this->certFilename = "";
	this->scaServer = 0;
	this->scaClient = 0;
	this->scaAttacker = 0;
	this->currentStep = 0;
	this->pButtonControl = 0;
	hi.init();
	this->isHybridEncryptedFileDeclared = false;
}

CDlgSideChannelAttackVisualizationHE::~CDlgSideChannelAttackVisualizationHE() {
	if(scaServer) delete scaServer;
	if(scaClient) delete scaClient;
	if(scaAttacker) delete scaAttacker;
	if(pButtonControl) delete pButtonControl;
	hi.free();
}

void CDlgSideChannelAttackVisualizationHE::DoDataExchange(CDataExchange* pDX) {
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ATTACKCONTROL, m_AttackControl);
	DDX_Control(pDX, IDC_BUTTON_ALLREMAININGSTEPS, m_ControlButtonAllSteps);
	DDX_Control(pDX, IDC_BUTTON_NEXTSINGLESTEP, m_ControlButtonNextStep);
	DDX_Control(pDX, IDC_PROGRESS_ATTACK, m_ControlAttackProgress);
	DDX_Control(pDX, IDC_ABARROW, m_ControlABArrow);
	DDX_Control(pDX, IDC_LIGHTS, m_ControlLights);
	DDX_Check(pDX, IDC_CHECK_DISABLEHELP, m_bShowInfoDialogues);
}

BOOL CDlgSideChannelAttackVisualizationHE::OnInitDialog() {
	try {
		CDialog::OnInitDialog();

		// *** VORWORT ***
		// Es gibt drei m�gliche Szenarien zu Beginn des Dialogs:
		//	(1) - KEINE Datei ge�ffnet/fokussiert
		//	(2) - G�LTIGE hybridverschl�sselte Datei ge�ffnet/fokussiert
		//  (3) - ANDERE (keine g�ltige)  hybridverschl�sselte Datei ge�ffnet/fokussiert
		// Anhand dieser Szenarien wird der "Startmodus" des Dialogs bestimmt, d.h.
		// welche zus�tzlichen Aktionen (Schl�sselerstellung usw.) im Vorfeld des
		// Seitenkanalangriffs n�tig sind

		// *** (1) ***
		// Der Benutzer hat vorher KEINE Datei angew�hlt, die als Ausgangsbasis f�r den
		// Seitenkanalangriff dienen k�nnte.
		if(!isFileDeclared)
			initMode = initMode | SCA_MODE_NO_FILE;		
		// *** (3) ***
		// Der Benutzer hat eine Datei focussiert, die KEINEN hybridverschl�sselten
		// Text beinhaltet. Somit muss entsprechend reagiert werden:
		else if(isFileDeclared && !isDocumentHybridEncrypted(initFile))
			initMode = initMode | SCA_MODE_INVALID_FILE;
		// *** (2) ***
		// An dieser Stelle hat der Anwender eine hybridverschl�sselte Datei angegeben
		// und kann nun mit der Durchf�hrung des Seitenkanalangriffs auf die besagte Datei beginnen
		else
			initMode = initMode | SCA_MODE_VALID_FILE;
		
		// Button-Kontrolle erstellen
		pButtonControl = new SideChannelAttackBitmapButtonControl(this);
		if(!pButtonControl) throw SCA_Error(E_SCA_MEMORY_ALLOCATION);

		// Ampel auf "normal" stellen
		setLights(SCA_LIGHTS_NORMAL);
		// Pfeil zwischen Alice und Bob (AB) auf "normal" stellen
		setABArrow(SCA_ABARROW_NORMAL);

		// Fortschrittsanzeige f�r den Angriff initialisieren
		// Obergrenze: [signifikante Bits] + 2
		// DEFAULT-WERT: 128 Bit


		if ( CT_OPEN_REGISTRY_SETTINGS( KEY_ALL_ACCESS, IDS_REGISTRY_SETTINGS, "SideChannelAttack" ) == ERROR_SUCCESS )
		{
			unsigned long u_significantBits = 128;
			CT_READ_REGISTRY_DEFAULT(u_significantBits, "BitlengthSecret", u_significantBits);
			if(!u_significantBits) throw SCA_Error(E_SCA_INTERNAL_ERROR);
			m_ControlAttackProgress.SetRange(0,(short)(u_significantBits+2));
			m_ControlAttackProgress.SetStep(1);
			m_ControlAttackProgress.SetPos(0);

			unsigned long u_flag_bShowInfoDialogues = (unsigned long)TRUE;
			CT_READ_REGISTRY_DEFAULT(u_flag_bShowInfoDialogues, "BitlengthSecret", u_flag_bShowInfoDialogues);
			this->m_bShowInfoDialogues = u_flag_bShowInfoDialogues;

			CT_CLOSE_REGISTRY();
		}
		else
		{
			// FIXME
		}

		// Buttons f�r Angriffssteuerung ausblenden
		cancelAttackCycle();

		// Informationsdialoge anzeigen? (schnellere Bedienung bei Bedarf)

		// Farben f�r Textfelder etc. festlegen
		this->m_greycolor=0x00C0C0C0; // RGB(0x00C0C0C0/*198,195,198*/);	// Standard-Grau
		this->m_greybrush.CreateSolidBrush(m_greycolor);
		this->m_blackcolor=RGB(0,0,0); // Schwarz
		this->m_blackbrush.CreateSolidBrush(m_blackcolor);

		UpdateData(false);
		
		return FALSE;  // return TRUE unless you set the focus to a control
					  // EXCEPTION: OCX-Eigenschaftenseiten sollten FALSE zur�ckgeben
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return true; }
}

void CDlgSideChannelAttackVisualizationHE::OnPaint() {
	CPaintDC dc(this);
	CBitmap bmp, *poldbmp;
	CDC memdc;
	bmp.LoadBitmap(IDB_SCA_MAINBACKGROUND);
	memdc.CreateCompatibleDC(&dc);
	poldbmp = memdc.SelectObject(&bmp);
	dc.BitBlt(0, 0, 981, 658, &memdc, 0, 0, SRCCOPY);
	memdc.SelectObject(poldbmp);
}

HBRUSH CDlgSideChannelAttackVisualizationHE::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
	HBRUSH hbr;
	switch (nCtlColor) 
	{ 
		case CTLCOLOR_MSGBOX: 
		case CTLCOLOR_EDIT: 
		case CTLCOLOR_BTN:
		case CTLCOLOR_STATIC:
		switch (pWnd->GetDlgCtrlID())  
		{     
			case IDC_CONTROLFRAME:
			case IDC_ALICEFRAME:
			case IDC_BOBFRAME:
			case IDC_TRUDYFRAME:
			case IDC_ATTACKCONTROL:
			case IDC_ATTACKPROGRESSLABEL:
			case IDC_PROGRESS_ATTACK:
			case IDC_BUTTON_NEXTSINGLESTEP:
			case IDC_BUTTON_ALLREMAININGSTEPS:
			case IDCLOSE:
			case IDC_CHECK_DISABLEHELP:
			case IDOK:
				pDC->SetBkColor(m_greycolor);
				pDC->SetTextColor(m_blackcolor);
				hbr = (HBRUSH)(m_greybrush);
				break;
			default:    
				hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor); 
			break;
		}
		break;
	default:  
		hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor); 
	}
	return hbr;
}

void CDlgSideChannelAttackVisualizationHE::updateGUI(int b) {
	try {
		pButtonControl->PerformAction(b);
		UpdateData(false);
	}
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::setEncryptedFile(const char *file) {
	try
	{
		initFile = file;
		// set file as declared only if it's not empty
		CrypTool::ByteString byteStringTemp;
		byteStringTemp.readFromFile(file);
		isFileDeclared = (byteStringTemp.getByteLength() > 0);
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::setHybridEncryptedFileInfo(HybridEncryptedFileInfo &_hi) {
	try {
		this->hi = _hi;
		this->isHybridEncryptedFileDeclared = true;
	}
	catch(SCA_Error &e) { CreateErrorMessage(e); return; };
}

void CDlgSideChannelAttackVisualizationHE::setInitFileTitle(const char *title) {
	try {
		this->initFileTitle = title;
	}
	catch(SCA_Error &e) { CreateErrorMessage(e); return; };	
}

bool CDlgSideChannelAttackVisualizationHE::isDocumentHybridEncrypted(const char *infile) {
	return CDlgHybridDecryptionDemo::isDocumentHybridEncrypted(infile);
}

void CDlgSideChannelAttackVisualizationHE::OnIntroduction() {
	try
	{
		if(this->m_bShowInfoDialogues)
		{
			// Einf�hrungsdialog anzeigen [weiter nichts]
			CDlgSideChannelAttackVisualizationHEIntroduction dlg;

			if(dlg.DoModal() != IDOK)
				return;
		}

		// Steuerelemente f�r Angriff "ausblenden"
		cancelAttackCycle();
		// RESET
		m_ControlAttackProgress.SetPos(0);
		// Pfeil zwischen Alice und Bob (AB) auf "normal" stellen
		setABArrow(SCA_ABARROW_NORMAL);
		
		// ** Akteure erstellen **
		// Falls zuvor bereits Akteure erstellt wurden, m�ssen diese gel�scht werden
		if(scaServer) delete scaServer;
		scaServer = new SCA_Server();
		if(!scaServer) throw SCA_Error(E_SCA_MEMORY_ALLOCATION);
		if(scaClient) delete scaClient;
		scaClient = new SCA_Client();
		if(!scaClient) throw SCA_Error(E_SCA_MEMORY_ALLOCATION);
		if(scaAttacker) delete scaAttacker;
		scaAttacker = new SCA_Attacker();
		if(!scaAttacker) throw SCA_Error(E_SCA_MEMORY_ALLOCATION);

		// Anzeige aktualisieren (NUR bei Druck auf Ok)
		updateGUI(0);
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::OnPreparations() {
	try
	{
		// forward decs
		CString serverPublicKey;
		CString serverModulus;
		
		// Vorbereitungsdialog anzeigen und "einstellen" (initMode, initFile)
		CDlgSideChannelAttackVisualizationHEPreparations dlg;
		dlg.setInitMode(initMode);
		dlg.setInitFile(initFile);
		dlg.setInitFileTitle(initFileTitle);
		if(dlg.DoModal() == IDOK)
		{
			// Steuerelemente f�r Angriff "ausblenden"
			cancelAttackCycle();			
			// Pfeil zwischen Alice und Bob (AB) auf "normal" stellen
			setABArrow(SCA_ABARROW_NORMAL);
			// an dieser Stelle wird unter <targetFile> der Pfad zur hybridverschl�sselten
			// Datei angegeben, auf die der Angriff ausgef�hrt werden soll.
			targetFile = dlg.getFinalHybEncFile();
			// acquire certificate serial
			certificateSerial = dlg.getCertificateSerial();
			// set server exponent e and server modulus n
			CrypTool::Cryptography::Asymmetric::CertificateStore::instance().getUserCertificateStringRSAPublicKeyE(certificateSerial, 10, serverPublicKey);
			CrypTool::Cryptography::Asymmetric::CertificateStore::instance().getUserCertificateStringRSAPublicKeyN(certificateSerial, 10, serverModulus);

			// RESET
			scaClient->cancelTransmission();
			scaServer->cancelReceptions();
			scaAttacker->cancelInterception();
			scaAttacker->cancelAttack();
			m_ControlAttackProgress.SetPos(0);

			// *** ACHTUNG ***
			// An dieser Stelle befindet sich der Pfad zur hybridverschl�sselten Datei in <certFilename>
			// und die ben�tigten Variablen f�r e,d,N sind gesetzt
			
			// Struktur mit Informationen aus hybridverschl�sselter Datei f�llen
			extractHybridEncryptedFileInformation((char*)(LPCTSTR)targetFile, hi);

			// *** ALICE ***
			// Entweder hat Alice die hybridverschl�sselte Datei SELBST erstellt, oder
			// die hybridverschl�sselte Datei existierte bereits zu Beginn der
			// Visualisierung und Alice schickt diese lediglich an Bob.
			if(dlg.isExistingHybEncFileUsed()) scaClient->useExistingHybEncFile(hi);
			else scaClient->useCreatedHybEncFile(hi, (char*)(LPCTSTR)dlg.getOriginalSessionKey());
			// *** BOB ***
			// Initialisierung der f�r die RSA-Entschl�sselung n�tigen Parameter
			scaServer->setPublicKey((char*)(LPCTSTR)serverPublicKey);
			scaServer->setModulus((char*)(LPCTSTR)serverModulus);
			// *** TRUDY ***
			// Trudy besorgt sich den �ffentlichen Schl�ssel von Bob
			scaAttacker->setTargetPublicKey((char*)(LPCTSTR)serverPublicKey, (char*)(LPCTSTR)serverModulus);

			// Anzeige aktualisieren (NUR bei Druck auf Ok)
			updateGUI(1);
		 }
		
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::OnMessagetransmission() {
	try
	{
		if(this->m_bShowInfoDialogues)
		{
			// Informationsdialog anzeigen
			CDlgSideChannelAttackVisualizationHEMessageTransmission dlg;	

			if(dlg.DoModal() != IDOK)
				return;
		}

		// Steuerelemente f�r Angriff "ausblenden"
		cancelAttackCycle();
		// RESET
		scaClient->cancelTransmission();
		scaServer->cancelReceptions();
		scaAttacker->cancelInterception();
		scaAttacker->cancelAttack();
		m_ControlAttackProgress.SetPos(0);

		// Pfeil zwischen Alice und Bob (AB) auf "Nachricht �bertragen" stellen
		setABArrow(SCA_ABARROW_TRANSMISSION);
		
		// Timer f�r Animationsabl�ufe setzen (siehe Funktion OnTimer)
		if(!SetTimer(SCA_TIMEREVENT_AB_TRANSMISSION, 50, 0))
			throw SCA_Error(E_SCA_TIMER_NOT_AVAILABLE);

		// Anzeige aktualisieren (nur bei Druck auf Ok)
		updateGUI(2);
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::OnMessagereception() {
	try
	{
	
		CString pin;
		CString serverPrivateKey;

		// PSE-PIN-Abfrage (f�r Zugang zu privatem Schl�ssel)
		while(1)
		{
			// PIN-Eingabedialog erstellen
			CDlgSideChannelAttackVisualizationHEPSEPINPrompt prompt;

			// Falls der Benutzer ABBRECHEN gedr�ckt hat, keine weiteren Aktionen durchf�hren, d.h.
			// die SCHLEIFE VERLASSEN und dem Benutzer eine entsprechende Nachricht zukommen lassen, dass
			// ohne die eingegebene PIN eine Angriffsvisualisierung unm�glich ist
			if(prompt.DoModal() == IDCANCEL)
			{
				LoadString(AfxGetInstanceHandle(), IDS_SCA_PSE_IS_NEEDED, pc_str, STR_LAENGE_STRING_TABLE);
				MessageBox(pc_str, "CrypTool", MB_OK);
				return;
			}

			// Falls der Benutzer OK gedr�ckt hat, mit den weiteren Vorbereitungen fortfahren...
			pin = prompt.getPin();

			// try to acquire the private key (in decimal format) using the provided PIN
			if (!CrypTool::Cryptography::Asymmetric::CertificateStore::instance().getUserCertificateStringRSAPrivateKeyD(certificateSerial, pin, 10, serverPrivateKey)) {
				// ERROR: falsche PIN, in Endlosschleife weitermachen
				LoadString(AfxGetInstanceHandle(), IDS_SCA_PSE_WRONG_PIN, pc_str, STR_LAENGE_STRING_TABLE);
				MessageBox(pc_str, "CrypTool", MB_OK);
			}
			else
			{
				// ALLES OK: PIN korrekt, Endlosschleife verlassen und somit mit Vorbereitungen fortfahren
				break;
			}
		}

		// RESET
		scaClient->cancelTransmission();
		scaServer->cancelReceptions();
		scaAttacker->cancelInterception();
		scaAttacker->cancelAttack();
		m_ControlAttackProgress.SetPos(0);

		// *** BOB ***
		scaServer->setPrivateKey((char*)(LPCTSTR)serverPrivateKey);
		scaServer->setPSEData((char*)(LPCTSTR)certFilename, (char*)(LPCTSTR)pin);

		// Nachricht �BERTRAGEN/EMPFANGEN
		scaServer->receiveHybridEncryptedFile(scaClient->transmitHybEncFile());

		// Anzeige aktualisieren (nur bei Druck auf Ok)
		updateGUI(9);
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::OnMessageinterception() {
	try
	{
		if(this->m_bShowInfoDialogues)
		{
			// Informationsdialog anzeigen
			CDlgSideChannelAttackVisualizationHEMessageInterception dlg;

			if(dlg.DoModal() != IDOK)
				return;
		}
		
		// RESET
		scaAttacker->cancelInterception();
		scaAttacker->cancelAttack();
		m_ControlAttackProgress.SetPos(0);
		// Steuerelemente f�r Angriff "ausblenden"
		cancelAttackCycle();

		// Pfeil zwischen Alice und Bob (AB) auf "Nachricht abfangen" stellen
		setABArrow(SCA_ABARROW_INTERCEPTION);
		// Nachricht ABFANGEN
		scaAttacker->interceptHybridEncryptedFile(scaClient->getHybEncFile());

		// Anzeige aktualisieren
		updateGUI(3);
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::OnAttackcycle() {
	try
	{
		// ACHTUNG
		// Abfangen, falls es sich um einen NULLSCHL�SSEL handelt, da der 
		// Angriff dann nicht m�glich ist (Folge: Endlosschleife/Speicherloch)
		if(isNullKey(&hi.sessionKeyEncrypted))
		{
			LoadString(AfxGetInstanceHandle(), IDS_SCA_SESSIONKEYISNULL, pc_str, STR_LAENGE_STRING_TABLE);
			MessageBox(pc_str, "CrypTool", MB_OK);
			return;
		}

		if(this->m_bShowInfoDialogues)
		{
		
			// Informationsdialog anzeigen
			CDlgSideChannelAttackVisualizationHEAttackCycle dlg;

			if(dlg.DoModal() != IDOK)
				return;
		}

		// Anzeige BEREITS HIER aktualisieren, damit roter Button
		// nicht den Anwender ablenkt
		updateGUI(4);

		// Pfeil zwischen Alice und Bob (AB) auf "normal" stellen
		setABArrow(SCA_ABARROW_NORMAL);

		// RESET
		scaAttacker->cancelAttack();
		scaServer->cancelAttackerReceptions();
		m_ControlAttackProgress.SetPos(0);
		cancelAttackCycle();

		if(this->m_bShowInfoDialogues)
		{
			// Anwender fragen, ob er den Angriff
			//  a) IM EINZELSCHRITTMODUS oder
			//  b) IN EINEM EINZIGEN DURCHLAUF
			// durchf�hren m�chte
			LoadString(AfxGetInstanceHandle(), IDS_SCA_ASKFORATTACKMODE, pc_str, STR_LAENGE_STRING_TABLE);
			const int singleStepMode = AfxMessageBox(pc_str, MB_YESNO);

			// EINZELSCHRITTMODUS
			if(singleStepMode == IDYES)
			{
				startAttackCycle();
				return;
			}
			// EIN EINZIGER DURCHLAUF
			else
			{
				SHOW_HOUR_GLASS
				while(!scaAttacker->isDone())
				{
					HybridEncryptedFileInfo hif = scaAttacker->nextHybridEncryptedFile();
					if(scaAttacker->isDone()) break;
					bool response = scaServer->receiveHybridEncryptedFile(hif);
					scaAttacker->processServerResponse(response);

					// Fortschrittsanzeige "updaten"
					m_ControlAttackProgress.StepIt();
				}
				HIDE_HOUR_GLASS
			}
		
			// in case ALL answers by the server were negative, obviously the attack failed;
			// the reason probably is the fact that the keyword ("Alice" by default) was not
			// part of the file that was attacked
			if(scaServer->getNumberOfPositiveResponses() == 0) {
				// first of all, get the current keyword from the registry
				CString keyword;
				if ( theApp.localRegistry.Open(HKEY_CURRENT_USER, "Software\\CrypTool\\Settings",KEY_READ) == ERROR_SUCCESS )
				{
					unsigned long u_length = 1024;
					char c_SCA_keyWord[1025];
					if (ERROR_SUCCESS == theApp.localRegistry.QueryValue(c_SCA_keyWord, "SCA_Keyword", &u_length) )			
						keyword = c_SCA_keyWord;
					else
						keyword.LoadStringA( IDS_SCA_KEYWORD );
				}
				// if we can't access the registry, we default to the keyword "Alice"
				else {
					keyword.LoadStringA( IDS_SCA_KEYWORD );
				}
				// now let the user know why the attack probably failed
				LoadString(AfxGetInstanceHandle(), IDS_SCA_ATTACK_FAILED_BECAUSE_OF_MISSING_KEYWORD, pc_str, STR_LAENGE_STRING_TABLE);
            CString message;
				MessageBox(message, "CrypTool");
			}
			// notify user that the attack was successful
			else {
				CDlgSideChannelAttackVisualizationHEFinished fin;
				fin.DoModal();
			}
		}
		else
		{
			startAttackCycle();
			return;
		}
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::OnReport() {
	try
	{
		// wg. m�glichen Inkosistenzen das Erstellen des Reports
		// erst NACH BEENDIGUNG des Angriffs zulassen!
		if(!scaAttacker->isDone())
		{
			LoadString(AfxGetInstanceHandle(), IDS_SCA_REPORTNOTYETGENERATED, pc_str, STR_LAENGE_STRING_TABLE);
			MessageBox(pc_str, "CrypTool");
			return;
		}

		if(this->m_bShowInfoDialogues)
		{
			// Informationsdialog anzeigen
			CDlgSideChannelAttackVisualizationHEReport dlg;

			if(dlg.DoModal() != IDOK)
				return;
		}

		// REPORT GENERIEREN
		// =================
	
		char filename[CRYPTOOL_PATH_LENGTH];
		GetTmpName(filename, "cry", ".txt");
		generateSCAReport(scaClient, scaServer, scaAttacker, filename);
		CAppDocument *NewDoc = theApp.OpenDocumentFileNoMRU(filename);

		// Message-Box �ber erfolgreichen Verlauf der Logtexterzeugung einblenden
		LoadString(AfxGetInstanceHandle(), IDS_SCA_REPORTGENERATED, pc_str, STR_LAENGE_STRING_TABLE);
		MessageBox(pc_str, "CrypTool");
		
		// Anzeige aktualisieren
		updateGUI(5);
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::OnAlice() {
	try
	{
		// Statusinformationen �ber Alice einblenden
		CDlgSideChannelAttackVisualizationHEAlice dlg(this);
		dlg.DoModal();
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::OnBob() {
	try
	{
		// Statusinformationen �ber Bob einblenden
		CDlgSideChannelAttackVisualizationHEBob dlg(this);
		dlg.DoModal();
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::OnTrudy() {
	try
	{
		// Statusinformationen �ber Trudy einblenden
		CDlgSideChannelAttackVisualizationHETrudy dlg(this);
		dlg.DoModal();
	}
	// Exceptions auffangen und entsprechende Fehlermeldungen erzeugen
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::CreateErrorMessage(SCA_Error &e) {
	int errorCode = e.getErrorCode();

	if(errorCode == E_SCA_INTERNAL_ERROR)
	{
		LoadString(AfxGetInstanceHandle(), IDS_SCA_E_INTERNAL_ERROR, pc_str, STR_LAENGE_STRING_TABLE);
		MessageBox(pc_str, "CrypTool", MB_ICONWARNING);
		return;
	}
	if(errorCode == E_SCA_MEMORY_ALLOCATION)
	{
		LoadString(AfxGetInstanceHandle(), IDS_SCA_E_MEMORY_ALLOCATION, pc_str, STR_LAENGE_STRING_TABLE);
		MessageBox(pc_str, "CrypTool", MB_ICONWARNING);
		return;
	}
	if(errorCode == E_SCA_HYBENCFILE_EXTRACTION)
	{
		LoadString(AfxGetInstanceHandle(), IDS_SCA_E_HYBENCFILE_EXTRACTION, pc_str, STR_LAENGE_STRING_TABLE);
		MessageBox(pc_str, "CrypTool", MB_ICONWARNING);
		return;
	}
	if(errorCode == E_SCA_ATTACK_CONDITIONS_NOT_MET)
	{
		LoadString(AfxGetInstanceHandle(), IDS_SCA_E_ATTACK_CONDITIONS_NOT_MET, pc_str, STR_LAENGE_STRING_TABLE);
		MessageBox(pc_str, "CrypTool", MB_ICONINFORMATION);
		return;
	}
	if(errorCode == E_SCA_MIRACL_ERROR)
	{
		LoadString(AfxGetInstanceHandle(), IDS_SCA_E_MIRACL_ERROR, pc_str, STR_LAENGE_STRING_TABLE);
		MessageBox(pc_str, "CrypTool", MB_ICONWARNING);
		return;
	}
	if(errorCode == E_SCA_WRONG_PSE_PIN)
	{
		LoadString(AfxGetInstanceHandle(), IDS_SCA_E_WRONG_PIN, pc_str, STR_LAENGE_STRING_TABLE);
		MessageBox(pc_str, "CrypTool", MB_ICONINFORMATION);
		return;
	}	
	if(errorCode == E_SCA_TIMER_NOT_AVAILABLE)
	{
		LoadString(AfxGetInstanceHandle(), IDS_SCA_E_TIMER_NOT_AVAILABLE, pc_str, STR_LAENGE_STRING_TABLE);
		MessageBox(pc_str, "CrypTool", MB_ICONINFORMATION);
		return;
	}

	// an dieser Stelle ist ein "unbekannter" Fehler ausgeworfen worden!
	LoadString(AfxGetInstanceHandle(), IDS_SCA_E_UNKNOWN_ERROR, pc_str, STR_LAENGE_STRING_TABLE);
	MessageBox(pc_str, "CrypTool", MB_ICONWARNING);
	return;
}

void CDlgSideChannelAttackVisualizationHE::setLights(int _mode) {
	// NORMAL
	if(_mode == SCA_LIGHTS_NORMAL)
	{
		this->m_ControlLights.UnLoad();
		this->m_ControlLights.Load(MAKEINTRESOURCE(IDR_SCA_LIGHTS_NORMAL), _T("GIF"));
		this->m_ControlLights.Stop();
		this->m_ControlLights.Draw();
	}
	// GR�N
	if(_mode == SCA_LIGHTS_GREEN)
	{
		this->m_ControlLights.UnLoad();
		this->m_ControlLights.Load(MAKEINTRESOURCE(IDR_SCA_LIGHTS_GREEN), _T("GIF"));
		this->m_ControlLights.Stop();
		this->m_ControlLights.Draw();
	}
	// ROT
	if(_mode == SCA_LIGHTS_RED)
	{
		this->m_ControlLights.UnLoad();
		this->m_ControlLights.Load(MAKEINTRESOURCE(IDR_SCA_LIGHTS_RED), _T("GIF"));
		this->m_ControlLights.Stop();
		this->m_ControlLights.Draw();
	}
}

void CDlgSideChannelAttackVisualizationHE::setABArrow(int _mode) {
	// NORMAL
	if(_mode == SCA_ABARROW_NORMAL)
	{
		this->m_ControlABArrow.UnLoad();
		this->m_ControlABArrow.Load(MAKEINTRESOURCE(IDR_SCA_ABARROWSTATIC), _T("GIF"));
		this->m_ControlABArrow.Stop();
		this->m_ControlABArrow.Draw();
	}
	// NACHRICHT WIRD �BERTRAGEN
	if(_mode == SCA_ABARROW_TRANSMISSION)
	{
		this->m_ControlABArrow.UnLoad();
		this->m_ControlABArrow.Load(MAKEINTRESOURCE(IDR_SCA_ABARROWTRANSMISSION), _T("GIF"));
		this->m_ControlABArrow.Stop();
		this->m_ControlABArrow.Draw();
	}
	// NACHRICHT WIRD ABGEFANGEN
	if(_mode == SCA_ABARROW_INTERCEPTION)
	{
		this->m_ControlABArrow.UnLoad();
		this->m_ControlABArrow.Load(MAKEINTRESOURCE(IDR_SCA_ABARROWINTERCEPTION), _T("GIF"));
		this->m_ControlABArrow.Stop();
		this->m_ControlABArrow.Draw();
	}
	// ANTWORT DES SERVERS: Fehlerhafte Entschl�sselung
	if(_mode == SCA_ABARROW_FAILURE)
	{
		this->m_ControlABArrow.UnLoad();
		this->m_ControlABArrow.Load(MAKEINTRESOURCE(IDR_SCA_ABARROWFAILURE), _T("GIF"));
		this->m_ControlABArrow.Stop();
		this->m_ControlABArrow.Draw();
	}
	// ANTWORT DES SERVERS: Erfolgreiche Entschl�sselung
	if(_mode == SCA_ABARROW_SUCCESS)
	{
		this->m_ControlABArrow.UnLoad();
		this->m_ControlABArrow.Load(MAKEINTRESOURCE(IDR_SCA_ABARROWSUCCESS), _T("GIF"));
		this->m_ControlABArrow.Stop();
		this->m_ControlABArrow.Draw();
	}
	// TRUDY (!!!) �bertr�gt Nachricht an Bob
	if(_mode == SCA_ABARROW_TRANSMISSION_TRUDYBOB)
	{
		this->m_ControlABArrow.UnLoad();
		this->m_ControlABArrow.Load(MAKEINTRESOURCE(IDR_SCA_ABARROWTRANSMISSION_TRUDYBOB), _T("GIF"));
		this->m_ControlABArrow.Stop();
		this->m_ControlABArrow.Draw();
	}
}

void CDlgSideChannelAttackVisualizationHE::OnClose() {
	EndDialog(IDCLOSE);	
}

void CDlgSideChannelAttackVisualizationHE::OnTimer(UINT nIDEvent) {
	// TRANSMISSION from ALICE to BOB
	if(nIDEvent == SCA_TIMEREVENT_AB_TRANSMISSION)
	{
		if(!m_ControlABArrow.IsPlaying())
		{
			// Falls Berechnung auf Serverseite noch nicht beendet,
			// noch KEINE Ampel anzeigen
			if(scaServer->getNumberOfReceptions() <= 0) 
				return;
			// ansonsten Timer l�schen und Ampel anzeigen (s.u.)
			else
				KillTimer(SCA_TIMEREVENT_AB_TRANSMISSION);
			
			// falls der Server die ERSTE Nachricht entschl�sseln konnte,
			// die Ampel GR�N leuchten lassen; andernfalls ROT
			if(scaServer->getFormerResponse(0))
				setLights(SCA_LIGHTS_GREEN);
			else
				setLights(SCA_LIGHTS_RED);
		}
	}

	// TRANSMISSION from TRUDY to BOB
	if(nIDEvent == SCA_TIMEREVENT_TB_TRANSMISSION)
	{
		if(!m_ControlABArrow.IsPlaying())
		{
			KillTimer(SCA_TIMEREVENT_TB_TRANSMISSION);

			if(scaServer->getFormerResponse(scaServer->getNumberOfReceptions()-1))
			{
				// Positive Antwort signalisieren
				setLights(SCA_LIGHTS_GREEN);
				// Antwort GR�N
				setABArrow(SCA_ABARROW_SUCCESS);
			}
			else
			{
				// negative Antwort signalisieren
				setLights(SCA_LIGHTS_RED);
				// Antwort ROT
				setABArrow(SCA_ABARROW_FAILURE);
			}
		}
	}
	
	CDialog::OnTimer(nIDEvent);
}

void CDlgSideChannelAttackVisualizationHE::startAttackCycle() {
	// Buttons einblenden
	m_ControlButtonNextStep.EnableWindow(true);
	m_ControlButtonNextStep.ShowWindow(SW_SHOW);

	m_ControlButtonAllSteps.EnableWindow(true);
	m_ControlButtonAllSteps.ShowWindow(SW_SHOW);

	m_AttackControl.EnableWindow(true);
	m_AttackControl.ShowWindow(SW_SHOW);

	// Fokus auf "n�chster Schritt" Button
	m_ControlButtonNextStep.SetFocus();
}

void CDlgSideChannelAttackVisualizationHE::cancelAttackCycle() {
	// Buttons ausblenden
	m_ControlButtonNextStep.EnableWindow(false);
	m_ControlButtonNextStep.ShowWindow(SW_HIDE);
	
	m_ControlButtonAllSteps.EnableWindow(false);
	m_ControlButtonAllSteps.ShowWindow(SW_HIDE);

	m_AttackControl.EnableWindow(false);
	m_AttackControl.ShowWindow(SW_HIDE);
}

void CDlgSideChannelAttackVisualizationHE::OnButtonNextsinglestep() {
	try {
		if(!scaAttacker->isDone())
		{
			SHOW_HOUR_GLASS
			HybridEncryptedFileInfo hif = scaAttacker->nextHybridEncryptedFile();
			if(scaAttacker->isDone())
			{
				// Steuerelemente f�r Angriff "ausblenden"
				cancelAttackCycle();
				// in case ALL answers by the server were negative, obviously the attack failed;
				// the reason probably is the fact that the keyword ("Alice" by default) was not
				// part of the file that was attacked
				if(scaServer->getNumberOfPositiveResponses() == 0) {
					// first of all, get the current keyword from the registry
					CString keyword;
					if ( theApp.localRegistry.Open(HKEY_CURRENT_USER, "Software\\CrypTool\\Settings",KEY_READ) == ERROR_SUCCESS )
					{
						unsigned long u_length = 1024;
						char c_SCA_keyWord[1025];
						if (ERROR_SUCCESS == theApp.localRegistry.QueryValue(c_SCA_keyWord, "SCA_Keyword", &u_length) )			
							keyword = c_SCA_keyWord;
						else
   						keyword.LoadStringA( IDS_SCA_KEYWORD );
					}
					// if we can't access the registry, we default to the keyword "Alice"
					else {
						keyword.LoadStringA( IDS_SCA_KEYWORD );
					}
					// now let the user know why the attack probably failed
               CString message;
               message.FormatMessage( IDS_SCA_ATTACK_FAILED_BECAUSE_OF_MISSING_KEYWORD, keyword.GetBuffer() );
					MessageBox(message, "CrypTool");
				}
				// notify user that the attack was successful
				else {
					CDlgSideChannelAttackVisualizationHEFinished fin;
					fin.DoModal();
				}
				// Anzeige aktualisieren
				updateGUI(4);
				return;
			}
			bool response = scaServer->receiveHybridEncryptedFile(hif);
			scaAttacker->processServerResponse(response);

			// Fortschrittsanzeige "updaten"
			m_ControlAttackProgress.StepIt();
			HIDE_HOUR_GLASS	

			// Animation abspielen: �bertragung Trudy->Bob
			setABArrow(SCA_ABARROW_TRANSMISSION_TRUDYBOB);
				
			// Timer f�r Animationsabl�ufe setzen (siehe Funktion OnTimer)
			if(!SetTimer(SCA_TIMEREVENT_TB_TRANSMISSION, 50, 0))
					throw SCA_Error(E_SCA_TIMER_NOT_AVAILABLE);
		}
	}
	// handle exceptions
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::OnButtonAllremainingsteps() {
	try {
		SHOW_HOUR_GLASS
		while(!scaAttacker->isDone())
		{
			HybridEncryptedFileInfo hif = scaAttacker->nextHybridEncryptedFile();
			if(scaAttacker->isDone()) break;
			bool response = scaServer->receiveHybridEncryptedFile(hif);
			scaAttacker->processServerResponse(response);

			// Fortschrittsanzeige "updaten"
			m_ControlAttackProgress.StepIt();
		}
		HIDE_HOUR_GLASS

		// Steuerelemente f�r Angriff "ausblenden"
		cancelAttackCycle();
		// in case ALL answers by the server were negative, obviously the attack failed;
		// the reason probably is the fact that the keyword ("Alice" by default) was not
		// part of the file that was attacked
		if(scaServer->getNumberOfPositiveResponses() == 0) {
			// first of all, get the current keyword from the registry
			CString keyword;
			if ( theApp.localRegistry.Open(HKEY_CURRENT_USER, "Software\\CrypTool\\Settings",KEY_READ) == ERROR_SUCCESS )
			{
				unsigned long u_length = 1024;
				char c_SCA_keyWord[1025];
				if (ERROR_SUCCESS == theApp.localRegistry.QueryValue(c_SCA_keyWord, "SCA_Keyword", &u_length) )			
					keyword = c_SCA_keyWord;
				else
					keyword.LoadStringA( IDS_SCA_KEYWORD );
			}
			// if we can't access the registry, we default to the keyword "Alice"
			else {
				keyword.LoadStringA( IDS_SCA_KEYWORD );
			}
			// now let the user know why the attack probably failed
         CString message;
         message.FormatMessage( IDS_SCA_ATTACK_FAILED_BECAUSE_OF_MISSING_KEYWORD, keyword.GetBuffer() );
			MessageBox(message, "CrypTool");
		}
		// notify user that the attack was successful
		else {
			CDlgSideChannelAttackVisualizationHEFinished fin;
			fin.DoModal();
		}
		// Anzeige aktualisieren
		updateGUI(4);
		return;
	}
	// handle exceptions
	catch(SCA_Error& e) { CreateErrorMessage(e); return; }
}

void CDlgSideChannelAttackVisualizationHE::OnCheckDisablehelp() {
	UpdateData(true);

	if ( CT_OPEN_REGISTRY_SETTINGS( KEY_WRITE, IDS_REGISTRY_SETTINGS ) == ERROR_SUCCESS )
	{
		CT_WRITE_REGISTRY(unsigned long(this->m_bShowInfoDialogues), "SCA_InfoDialogues");
		CT_CLOSE_REGISTRY();
	}
	else
	{
		// FIXME
	}
}

BEGIN_MESSAGE_MAP(CDlgSideChannelAttackVisualizationHE, CDialog)
	ON_BN_CLICKED(IDC_INTRODUCTION, OnIntroduction)
	ON_BN_CLICKED(IDC_PREPARATIONS, OnPreparations)
	ON_BN_CLICKED(IDC_MESSAGETRANSMISSION, OnMessagetransmission)
	ON_BN_CLICKED(IDC_MESSAGEINTERCEPTION, OnMessageinterception)
	ON_BN_CLICKED(IDC_ATTACKCYCLE, OnAttackcycle)
	ON_BN_CLICKED(IDC_REPORT, OnReport)
	ON_BN_CLICKED(IDC_ALICE, OnAlice)
	ON_BN_CLICKED(IDC_BOB, OnBob)
	ON_BN_CLICKED(IDC_TRUDY, OnTrudy)
	ON_BN_CLICKED(IDCLOSE, OnClose)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BUTTON_NEXTSINGLESTEP, OnButtonNextsinglestep)
	ON_BN_CLICKED(IDC_BUTTON_ALLREMAININGSTEPS, OnButtonAllremainingsteps)
	ON_BN_CLICKED(IDC_MESSAGERECEPTION, OnMessagereception)
	ON_BN_CLICKED(IDC_CHECK_DISABLEHELP, OnCheckDisablehelp)
	ON_WM_PAINT()
	ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

#include "DHTesp.h"

#include <WiFi.h>             //biblioteca com as funções WiFi
#include "PubSubClientExt.h" //ficheiro produzido por Luis Figueiredo para facilitar a utilização da "internet das coisas"

//deve ser colocado na mesma pasta do ficheiro .ino que se está a usar
//este ficheiro funciona unicamente para o broker da UBIDOTS. para outros brokers terá que ser alterado
//deve ser previamente instalada a biblioteca PubSubClient.h sem a qual a biblioteca PubSubClientExt.h não funciona


#define WIFISSID "HUAWEI P30 lite"                         //nome da rede WiFi a que se vai ligar
#define PASSWORD "***********"                            //Password da rede WiFi a que se vai ligar
#define TOKEN "BBFF-KVb1MOU9gPhRzdZAwO8TYan2rPtsJD" //TOKEN da UBIDOTS. Deve-se obter do site da UBIDOTS depois de se fazer o registo
#define MQTT_CLIENT_NAME "IVB3AQX003WD"             //12 carateres com letras ou números gerados aleatoriamente;
//devem ser diferentes para cada dispositivo. Não usar esta sequência
//sugestão: usar o site https://www.random.org/strings/

#define DEVICE_LABEL "esp32"                        // Nome do dispositivo que se pretende usar
char mqttBroker[] = "industrial.api.ubidots.com";   //endereço do broker da UBIDOTS
#define PORTO 1883                                  //porto do broker da UBIDOTS
#define INTERVALO 8000                             //Intervalo em milissegundos para o envio de dados para a UBIDOTS 
#define TIME_TO_RECONECT  500                       //tempo entre duas tentativas seguidas para restabelecer a ligação ao broker

DHTesp dht;
WiFiClient ubidots;                 //variável (objeto) para controlo da ligação WiFi
PubSubClientExt client(ubidots);    //variável (objeto) para controlo da ligação MQTT

#define pinLed 12         //pino onde se liga um led para testar o ligar e desligar através do site da UBIDOTS
#define dhtPin 27         //pino onde se liga o DHT11 para testar o controlo do brilho através do site da UBIDOTS

unsigned long tempo = 0;
unsigned long ti = 0;      //variável para controlar o tempo de envio de dados para o broker da UBIDOTS

unsigned long histereses = 1;       // variavel para controlar a histereses
unsigned long temperatura_referencia;
float temperatura;


void setup()
{
  int contador = 0;
  WiFi.disconnect();

  Serial.begin(115200);                 //inicia a porta série à velocidade de 115200bps
  WiFi.begin(WIFISSID, PASSWORD);       //inicia a ligação à rede WIFI com o nome WIFISSID e a password  PASSWORD

  Serial.print("\nEfetuando a ligação à rede  WiFi...");

  while (WiFi.status() != WL_CONNECTED) //ciclo para aguardar que seja estabelecia a ligação à rede WIFI
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nLigação WiFi estabelecida com sucesso");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());      //imprime o endereço IP privado atribuído ao ESP32


  client.setServer(mqttBroker, PORTO); //Configura o endereço do broker da UBIDOTS
  client.setCallback(callback);        //estabelece a função callback para ser chamada sempre que o broker enviar informações para este dispositivo
  //será dentro desta função que se analisará a informação recebida e em função dessa informação se decidirá o que fazer

  pinMode(pinLed, OUTPUT);                //define-se o pinLed como OUTPUT

  ti = millis();                        //inicia-se o contador de tempo inicial para o envio de dados para o broker.

  dht.setup(dhtPin, DHTesp::DHT11);     //inicializar o sensor DHT11 indicando qual o pino e o modelo DHt11
  Serial.println("DHT11 iniciado");

}


void loop()
{

  if (!client.connected())              //se deixou de haver uma ligação ao broker volta-se a estabelecê-la
    if (reconnect())
      subscrever();                      //subscreve as variáveis do broker que pretende ser notificado

  if (millis() - ti >= INTERVALO)       //se já passou o tempo definido para enviar dados
  {
    ti = millis();                      //atualiza-se o tempo inicial

    temperatura = dht.getTemperature();                         //Ler temperatura
    Serial.printf ("temperatura=%.1f\n", temperatura);

    client.init_send(DEVICE_LABEL);                   //inicia o processo de envio de dados para o broker do dispositivo DEVICE_LABEL
    client.add_point("temperatura", temperatura);     //adiciona um ponto ao broker da variável com o nome temperatura

    client.send_all(true);              //envia todos os pontos para o broker.
    //a mensagem a enviar terá o seguinte formato: /v1.6/devices/temperatura
    //se o parâmetro for true a função envia também para a porta série os dados enviados

  }
  if (temperatura > temperatura_referencia + histereses) {          //Histereses implementada
    digitalWrite(pinLed, HIGH);
  } else if (temperatura < temperatura_referencia - histereses) {
    digitalWrite(pinLed, LOW);
  }

  client.loop();                        //função que é fundamental ser chamada o máximo de vezes possivel. Não deve haver no loop qualquer delay
}

void callback(char *topic, byte *payload, unsigned int length) {
  //função que será chamada sempre que o site da UBIDOTS enviar dados para o dispositivo
  //topic: variável que conterá a identificação do dispositivo e da variável do site da UBIDOTS
  //payload: variável que conterá a informação enviado pelo site da UBIDOTS. A informação vem em carateres ASCII não terminados com \0
  //length: número de bytes que vêm na variável payload
 
  int valor = get_int(payload, length);             //função que converte os carateres ASCII recebidos num número
  if (client.verify(topic, DEVICE_LABEL, "referencia")) {  //função que verifica se o dispositivo e a variável são as indicadas em DEVICE_LABEL e "referencia"
    Serial.print("recebi referencia de temperatura do esp32 o valor ");
    Serial.println(valor);  //imprime o valor da referencia recebida
    temperatura_referencia = valor;
  }

}

void subscrever() {
  client.subscribeExt(DEVICE_LABEL, "referencia", true);     //subscreve a variável temperatura de referencia do DEVICE_LABEL. Se o último parâmetro for true envia para a porta série a mensagem da subscrição
  //o broker passará a enviar a este dispositivo qualquer alteração da variável da referencia DEVICE_LABEL

}

//função para restabelecer uma ligação ao broker da UBIDOTS
bool reconnect()
{
  static unsigned long time_reconect = 0;         //variável para controlar o tempo para voltar a tentar estabelecer a ligação
  static bool first_time = true;                  //variável para controlar as mensagens a apresentar

  if (millis() - time_reconect >= TIME_TO_RECONECT) { //se já passou o tempo TIME_TO_RECONECT
    time_reconect = millis();                     //atualiza a variável time_reconect com o tempo atual
    if (first_time) {                             //se é a primeira vez escreve uma mensagem
      Serial.println("Estabelecendo ligação MQTT");
      first_time = false;                         //altera a variável first_time para não voltar a escrever a mesma mensage
    }
    else
      Serial.print(".");                         //se não é a primeira vez escreve apenas .

    if (client.connect(MQTT_CLIENT_NAME, TOKEN, ""))  //tenta estabelecer a ligação ao broker
    {
      Serial.println("Connected");       //informa do estabelecimento da ligação ao broker
      first_time = true;                 //coloca a variável first_time a true para ficar pronta para a próxima ligação
      return (true) ;                    //indica que a ligação foi estabelecida
    }
    else
    {
      Serial.print("Erro na ligação MQTT, rc=");  //Indica a ocorrência de um erro na ligação
      Serial.print(client.state());

    }
  }
  return (false);                       //indica que a ligação ainda não foi estabelecida
}

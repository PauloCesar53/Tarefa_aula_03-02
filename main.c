/*
 *                    Funcionamento do programa 
 *O programa mostra números de 0 a 9 na matriz de lEDS 5x5, a depender da entrada de um desses 
 números no serial monitor,ao apertar o botão A o lED verde muda de estado, com a informação 
 sendo printada no serial monitor e no display da BitDogLab, o Botão B tem comportamento 
 similar ao botão A mas para funcionamento do led Azul. Os caracteres inseridos via serial monitor 
 aparecem no Display 
 *                    Tratamento de deboucing com interrupção 
 * A ativação dos botões A e B são feitas através de uma rotina de interrupção, sendo
 * implementada condição para tratar o efeito boucing na rotina.
*/

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
//definições da UART
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define IS_RGBW false
#define NUM_PIXELS 25
#define WS2812_PIN 7
#define Frames 10
#define LED_PIN_G 11
#define LED_PIN_B 12
#define Botao_A 5 // gpio do botão A na BitDogLab
#define Botao_B 6 // gpio do botão B na BitDogLab

//definição de variáveis que guardarão o estado atual de cada lED
bool led_on_G = false;// LED verde desligado
bool led_on_B = false;// LED azul desligado
int aux_G= 1;//variável auxiliar para indicar mudança de estado no display 
int aux_B= 1;

// Variável global para armazenar a cor (Entre 0 e 255 para intensidade)
uint8_t led_r = 0;  // Intensidade do vermelho
uint8_t led_g = 0; // Intensidade do verde
uint8_t led_b = 255; // Intensidade do azul


//variáveis globais 
static volatile int aux = 1; // posição do numero impresso na matriz, inicialmente imprime numero 5
static volatile uint32_t last_time_A = 0; // Armazena o tempo do último evento para Bot A(em microssegundos)
static volatile uint32_t last_time_B = 0; // Armazena o tempo do último evento para Bot B(em microssegundos)
char c;

bool led_buffer[NUM_PIXELS];// Variável (protótipo)
bool buffer_Numeros[Frames][NUM_PIXELS];// // Variável (protótipo) 
    
//protótipo funções que ligam leds da matriz 5x5
void atualiza_buffer(bool buffer[], bool b[][NUM_PIXELS], int c); ///protótipo função que atualiza buffer

static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}
void set_one_led(uint8_t r, uint8_t g, uint8_t b);//liga os LEDs escolhidos 

void gpio_irq_handler(uint gpio, uint32_t events);// protótipo função interrupção para os botões A e B com condição para deboucing
void Imprime_5X5(char car);// protótipo função que colocar números de zero a 9 na 5x5

int main()
{
    //Inicializando I2C . Usando 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);// Definindo a função do pino GPIO para I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);// Definindo a função do pino GPIO para I2C
    gpio_pull_up(I2C_SDA);// definindo como resistência de pull up
    gpio_pull_up(I2C_SCL);// definindo como resistência de pull up
    ssd1306_t ssd;// Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);// Inicializa o display
    ssd1306_config(&ssd);// Configura o display
    ssd1306_send_data(&ssd);// Envia os dados para o display
    bool cor = true;
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
  

    //inicializando PIO
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    
    stdio_init_all(); // Inicializa comunicação USB CDC para monitor serial

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
    // configuração led RGB verde
    gpio_init(LED_PIN_G);              // inicializa pino do led verde
    gpio_set_dir(LED_PIN_G, GPIO_OUT); // configrua como saída
    gpio_put(LED_PIN_G, 0);            // led inicia apagado
    
    // configuração led RGB azul
    gpio_init(LED_PIN_B);              // inicializa pino do led azul
    gpio_set_dir(LED_PIN_B, GPIO_OUT); // configrua como saída
    gpio_put(LED_PIN_B, 0);            // led inicia apagado
    
    // configuração botão A
    gpio_init(Botao_A);             // inicializa pino do botão A
    gpio_set_dir(Botao_A, GPIO_IN); // configura como entrada
    gpio_pull_up(Botao_A);          // Habilita o pull-up interno

    // configuração botão B
    gpio_init(Botao_B);             // inicializa pino do botão B
    gpio_set_dir(Botao_B, GPIO_IN); // configura como entrada
    gpio_pull_up(Botao_B);          // Habilita o pull-up interno

    atualiza_buffer(led_buffer, buffer_Numeros, aux); // atualiza buffer para numero 5
    set_one_led(led_r, led_g, led_b);               // forma numero 5 primeira vez

    // configurando a interrupção com botão na descida para o botão A
    gpio_set_irq_enabled_with_callback(Botao_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // configurando a interrupção com botão na descida para o botão B
    gpio_set_irq_enabled_with_callback(Botao_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    while (1)
    {
         cor = !cor;
        // ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 30); // Desenha uma string
        if (stdio_usb_connected())
        { // Certifica-se de que o USB está conectado
            if (scanf("%c", &c) == 1)
            {
                ssd1306_draw_char(&ssd, c, 50, 30);//desenha o caracter digitado via serial monitor
                Imprime_5X5(c);//Numero na matriz 5x5
            }
        ssd1306_send_data(&ssd); // Atualiza o display

        if(aux_G==2){//atualiza alteração LED verde display
        ssd1306_fill(&ssd, !cor);                     // Limpa o display
        ssd1306_draw_string(&ssd, "LED verde foi ligado", 8, 10); // Desenha uma string
        aux_G=1;
        }else if(aux_G==0){
        ssd1306_fill(&ssd, !cor);                     // Limpa o display
        ssd1306_draw_string(&ssd, "LED verde foi desligado", 8, 10); // Desenha uma string
        aux_G=1;
        }
        if(aux_B==2){//atualizar alteração LED verde display
        // Atualiza o conteúdo do display 
        ssd1306_fill(&ssd, !cor);// Limpa o display
        ssd1306_draw_string(&ssd, "LED azul foi ligado", 8, 10); // Desenha uma string
        aux_B=1;
        }else if(aux_B==0){
        ssd1306_fill(&ssd, !cor);                     // Limpa o display
        ssd1306_draw_string(&ssd, "LED azul foi desligado", 8, 10); // Desenha uma string
        aux_B=1;
        }
        sleep_ms(1000);

        
    }

    return 0;
    }
}

bool led_buffer[NUM_PIXELS] = { //Buffer para armazenar quais LEDs estão ligados matriz 5x5
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0};

bool buffer_Numeros[Frames][NUM_PIXELS] =//Frames que formam os números de 0 a 9
    {
        //{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24} referência para posição na BitDogLab matriz 5x5
        {0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0}, // para o número zero
        {0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0}, // para o número 1
        {0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0}, // para o número 2
        {0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0}, // para o número 3
        {0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0}, // para o número 4
        {0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0}, // para o número 5
        {0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0}, // para o número 6
        {0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0}, // para o número 7
        {0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0}, // para o número 8
        {0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0}  // para o número 9
};

// função que atualiza o buffer de acordo o número de 0 a 9
void atualiza_buffer(bool buffer[],bool b[][NUM_PIXELS], int c)
{
    for (int i = 0; i < NUM_PIXELS; i++)
    {
        buffer[i] = b[c][i];
    }
}

void set_one_led(uint8_t r, uint8_t g, uint8_t b)
{
    // Define a cor com base nos parâmetros fornecidos
    uint32_t color = urgb_u32(r, g, b);

    // Define todos os LEDs com a cor especificada
    for (int i = 0; i < NUM_PIXELS; i++)
    {
        if (led_buffer[i])
        {
            put_pixel(color); // Liga o LED com um no buffer
        }
        else
        {
            put_pixel(0); // Desliga os LEDs com zero no buffer
        }
    }
}

// função interrupção para os botões A e B com condição para deboucing
void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint32_t current_time = to_us_since_boot(get_absolute_time());//// Obtém o tempo atual em microssegundos
    if (gpio_get(Botao_A) == 0 &&  (current_time - last_time_A) > 200000)//200ms de boucing adiconado como condição 
    { // se botão A for pressionado e aux<9 incrementa aux em 1(próximo número) 
        last_time_A = current_time; // Atualiza o tempo do último evento
        led_on_G= !led_on_G;
        gpio_put(LED_PIN_G, led_on_G);
        if(led_on_G){
            printf("LED verde ligado\n");
            aux_G++;
        }else{
            printf("LED verde desligado\n");
            aux_G--;
        }
        //aux++;
        //atualiza_buffer(led_buffer, buffer_Numeros, aux); /// atualiza buffer
        //set_one_led(led_r, led_g, led_b);               // forma numero na matriz
    }
    if (gpio_get(Botao_B) == 0  && (current_time - last_time_B) > 200000)//200ms de boucing adiconado como condição 
    { // se botão B for pressionado e aux>0 decrementa aux em 1(número anterior)
        last_time_B = current_time; // Atualiza o tempo do último evento
        led_on_B= !led_on_B;
        gpio_put(LED_PIN_B, led_on_B);
        if(led_on_B){
            printf("LED azul ligado\n");
            aux_B++;
        }else{
            printf("LED azul desligado\n");
            aux_B--;
        }
        //aux--;
        //atualiza_buffer(led_buffer, buffer_Numeros, aux); // atualiza buffer
        ///set_one_led(led_r, led_g, led_b);               // forma número na matriz
    }
}
//função que colocar números de zero a 9 na 5x5
void Imprime_5X5(char car){
    if(car == '0'){
        atualiza_buffer(led_buffer, buffer_Numeros, 0); // atualiza buffer para numero 1
        set_one_led(led_r, led_g, led_b);//forma número 1 namatriz 
    }else if(car == '1'){
        atualiza_buffer(led_buffer, buffer_Numeros, 1); // atualiza buffer para numero 1
        set_one_led(led_r, led_g, led_b);//forma número 1 namatriz 
    }else if(car == '2'){
        atualiza_buffer(led_buffer, buffer_Numeros, 2); // atualiza buffer para numero 1
        set_one_led(led_r, led_g, led_b);//forma número 1 namatriz 
    }else if(car == '3'){
        atualiza_buffer(led_buffer, buffer_Numeros, 3); // atualiza buffer para numero 1
        set_one_led(led_r, led_g, led_b);//forma número 1 namatriz 
    }else if(car == '4'){
        atualiza_buffer(led_buffer, buffer_Numeros, 4); // atualiza buffer para numero 1
        set_one_led(led_r, led_g, led_b);//forma número 1 namatriz 
    }else if(car == '5'){
        atualiza_buffer(led_buffer, buffer_Numeros, 5); // atualiza buffer para numero 1
        set_one_led(led_r, led_g, led_b);//forma número 1 namatriz 
    }else if(car == '6'){
        atualiza_buffer(led_buffer, buffer_Numeros, 6); // atualiza buffer para numero 1
        set_one_led(led_r, led_g, led_b);//forma número 1 namatriz 
    }else if(car == '7'){
        atualiza_buffer(led_buffer, buffer_Numeros, 7); // atualiza buffer para numero 1
        set_one_led(led_r, led_g, led_b);//forma número 1 namatriz 
    }else if(car == '8'){
        atualiza_buffer(led_buffer, buffer_Numeros, 8); // atualiza buffer para numero 1
        set_one_led(led_r, led_g, led_b);//forma número 1 namatriz 
    }else if(car == '9'){
        atualiza_buffer(led_buffer, buffer_Numeros, 9); // atualiza buffer para numero 1
        set_one_led(led_r, led_g, led_b);//forma número 1 namatriz 
    }else{

    }

}
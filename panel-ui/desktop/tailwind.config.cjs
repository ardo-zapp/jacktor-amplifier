module.exports = {
  content: ['./index.html', './src/**/*.{ts,tsx,js,jsx}'],
  theme: {
    extend: {
      colors: {
        primary: '#00CFFF',
        accent: '#00E6FF',
        error: '#FF3355',
        background: '#080B0E'
      },
      fontFamily: {
        sans: ['\"Roboto Condensed\"', 'sans-serif'],
        display: ['Orbitron', 'sans-serif']
      }
    }
  },
  plugins: []
};

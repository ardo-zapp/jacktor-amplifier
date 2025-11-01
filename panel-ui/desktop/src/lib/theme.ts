import React from 'react';

export const baseTheme = {
  colors: {
    primary: '#00CFFF',
    accent: '#00E6FF',
    error: '#FF3355',
    background: '#080B0E',
  },
  spacing: {
    gutter: 16,
  },
};

export const ThemeContext = React.createContext(baseTheme);
